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
    "Set text direction", "DIRECTION" },
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

static char *
get_output_file (const char *test_file,
                 const char *extension)
{
  const char *output_dir = get_output_dir ();
  char *result, *base;

  base = g_path_get_basename (test_file);
  if (g_str_has_suffix (base, ".ui"))
    base[strlen (base) - strlen (".ui")] = '\0';

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

  if (g_str_has_suffix (test_file, ".ui"))
    g_string_append_len (file, test_file, strlen (test_file) - strlen (".ui"));
  else
    g_string_append (file, test_file);
  
  g_string_append (file, extension);

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

static void
check_for_draw (GdkEvent *event, gpointer loop)
{
  if (event->type == GDK_EXPOSE)
    {
      g_idle_add (quit_when_idle, loop);
      gdk_event_handler_set ((GdkEventFunc) gtk_main_do_event, NULL, NULL);
    }

  gtk_main_do_event (event);
}

static cairo_surface_t *
snapshot_widget (GtkWidget *widget, SnapshotMode mode)
{
  cairo_surface_t *surface;
  cairo_pattern_t *bg;
  GMainLoop *loop;
  cairo_t *cr;

  g_assert (gtk_widget_get_realized (widget));

  surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               gtk_widget_get_allocated_width (widget),
                                               gtk_widget_get_allocated_height (widget));

  loop = g_main_loop_new (NULL, FALSE);
  /* We wait until the widget is drawn for the first time.
   * We can not wait for a GtkWidget::draw event, because that might not
   * happen if the window is fully obscured by windowed child widgets.
   * Alternatively, we could wait for an expose event on widget's window.
   * Both of these are rather hairy, not sure what's best. */
  gdk_event_handler_set (check_for_draw, loop, NULL);
  g_main_loop_run (loop);

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

static cairo_surface_t *
snapshot_ui_file (const char *ui_file)
{
  GtkWidget *window;
  GtkBuilder *builder;
  GError *error = NULL;

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, ui_file, &error);
  g_assert_no_error (error);
  window = builder_get_toplevel (builder);
  g_object_unref (builder);
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
get_surface_size (cairo_surface_t *surface,
                  int             *width,
                  int             *height)
{
  GdkRectangle area;
  cairo_t *cr;

  cr = cairo_create (surface);
  if (!gdk_cairo_get_clip_rectangle (cr, &area))
    {
      g_assert_not_reached ();
    }

  g_assert (area.x == 0 && area.y == 0);
  g_assert (area.width > 0 && area.height > 0);

  *width = area.width;
  *height = area.height;
}

static cairo_surface_t *
coerce_surface_for_comparison (cairo_surface_t *surface,
                               int              width,
                               int              height)
{
  cairo_surface_t *coerced;
  cairo_t *cr;

  coerced = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        width,
                                        height);
  cr = cairo_create (coerced);
  
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  g_assert (cairo_surface_status (coerced) == CAIRO_STATUS_SUCCESS);

  return coerced;
}

/* Compares two CAIRO_FORMAT_ARGB32 buffers, returning NULL if the
 * buffers are equal or a surface containing a diff between the two
 * surfaces.
 *
 * This function should be rewritten to compare all formats supported by
 * cairo_format_t instead of taking a mask as a parameter.
 *
 * This function is originally from cairo:test/buffer-diff.c.
 * Copyright © 2004 Richard D. Worth
 */
static cairo_surface_t *
buffer_diff_core (const guchar *buf_a,
                  int           stride_a,
		  const guchar *buf_b,
                  int           stride_b,
		  int		width,
		  int		height)
{
  int x, y;
  guchar *buf_diff = NULL;
  int stride_diff = 0;
  cairo_surface_t *diff = NULL;

  for (y = 0; y < height; y++)
    {
      const guint32 *row_a = (const guint32 *) (buf_a + y * stride_a);
      const guint32 *row_b = (const guint32 *) (buf_b + y * stride_b);
      guint32 *row = (guint32 *) (buf_diff + y * stride_diff);

      for (x = 0; x < width; x++)
        {
          int channel;
          guint32 diff_pixel = 0;

          /* check if the pixels are the same */
          if (row_a[x] == row_b[x])
            continue;
        
          if (diff == NULL)
            {
              diff = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                                 width,
                                                 height);
              g_assert (cairo_surface_status (diff) == CAIRO_STATUS_SUCCESS);
              buf_diff = cairo_image_surface_get_data (diff);
              stride_diff = cairo_image_surface_get_stride (diff);
              row = (guint32 *) (buf_diff + y * stride_diff);
            }

          /* calculate a difference value for all 4 channels */
          for (channel = 0; channel < 4; channel++)
            {
              int value_a = (row_a[x] >> (channel*8)) & 0xff;
              int value_b = (row_b[x] >> (channel*8)) & 0xff;
              guint diff;

              diff = ABS (value_a - value_b);
              diff *= 4;  /* emphasize */
              if (diff)
                diff += 128; /* make sure it's visible */
              if (diff > 255)
                diff = 255;
              diff_pixel |= diff << (channel*8);
            }

          if ((diff_pixel & 0x00ffffff) == 0)
            {
              /* alpha only difference, convert to luminance */
              guint8 alpha = diff_pixel >> 24;
              diff_pixel = alpha * 0x010101;
            }
          
          row[x] = diff_pixel;
      }
  }

  return diff;
}

static cairo_surface_t *
compare_surfaces (const char *test_file,
                  cairo_surface_t *surface1,
                  cairo_surface_t *surface2)
{
  int w1, h1, w2, h2, w, h;
  cairo_surface_t *diff;
  
  get_surface_size (surface1, &w1, &h1);
  get_surface_size (surface2, &w2, &h2);
  w = MAX (w1, w2);
  h = MAX (h1, h2);
  surface1 = coerce_surface_for_comparison (surface1, w, h);
  surface2 = coerce_surface_for_comparison (surface2, w, h);

  diff = buffer_diff_core (cairo_image_surface_get_data (surface1),
                           cairo_image_surface_get_stride (surface1),
                           cairo_image_surface_get_data (surface2),
                           cairo_image_surface_get_stride (surface2),
                           w, h);

  return diff;
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

  diff_image = compare_surfaces (ui_file, ui_image, reference_image);

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

