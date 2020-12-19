/*
 * Copyright © 2020 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include <ottie/ottie.h>
#include <gtk/gtk.h>

#include <glib/gi18n.h>

static GskRenderNode *
snapshot_paintable (GdkPaintable *paintable,
                    int           width,
                    int           height)
{
  GtkSnapshot *snapshot;

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable, snapshot, width, height);
  return gtk_snapshot_free_to_node (snapshot);
}

static void
draw_paintable (GdkPaintable    *paintable,
                cairo_surface_t *surface)
{
  cairo_t *cr;
  GskRenderNode *node;

  node = snapshot_paintable (paintable, 
                             cairo_image_surface_get_width (surface),
                             cairo_image_surface_get_height (surface));

  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  if (node)
    {
      gsk_render_node_draw (node, cr);
      gsk_render_node_unref (node);
    }

  cairo_destroy (cr);
}

static gboolean
save_paintable_to_png (GdkPaintable *paintable,
                       const char   *filename,
                       int           width,
                       int           height)
{
  cairo_surface_t *surface;
  cairo_status_t status;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  draw_paintable (paintable, surface);

  status = cairo_surface_write_to_png (surface, filename);
  if (status != CAIRO_STATUS_SUCCESS)
    g_printerr (_("Failed to save to \"%s\": %s\n"), filename, cairo_status_to_string (status));

  cairo_surface_destroy (surface);

  return status == CAIRO_STATUS_SUCCESS;
}

static gboolean
save_paintable_to_node (GdkPaintable *paintable,
                        const char   *filename,
                        int           width,
                        int           height)
{
  GskRenderNode *node;
  GError *error = NULL;
  gboolean result;

  node = snapshot_paintable (GDK_PAINTABLE (paintable), width, height);
  if (node == NULL)
    result = g_file_set_contents (filename, "", 0, &error);
  else
    {
      result = gsk_render_node_write_to_file (node, filename, &error);
      gsk_render_node_unref (node);
    }

  if (!result)
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
    }

  return result;
}

static int
usage (void)
{
  g_print (_("Usage:\n"
             "ottie [COMMAND] [OPTION…] FILEs\n"
             "  Perform various tasks on a Lottie file.\n"
             "\n"
             "ottie image [OPTION…] FILE IMAGE-FILE\n"
             "  Save a PNG of the given input file.\n"
             "  --time=[timestamp]  Forward to [timestamp] seconds\n"
             "  --size=[max]        Resize larger dimension to [max]\n"
             "\n"
             "ottie node [OPTION…] FILE NODE-FILE\n"
             "  Save a rendernode file of the given input file.\n"
             "  --time=[timestamp]  Forward to [timestamp] seconds\n"
             "  --size=[max]        Resize larger dimension to [max]\n"
             "\n"
             "ottie video [OPTION…] FILE VIDEO-FILE\n"
             "  Save a WebM of the given input file.\n"
             "  --size=[max]        Resize larger dimension to [max]\n"
             "\n"
             "ottie show [OPTION…] FILE\n"
             "  Show a small video player for the given file.\n"
             "\n"
             "Perform various tasks on Lottie files.\n"));

  return 1;
}

static void
paintable_get_size (OttiePaintable *paintable,
                    int             desired,
                    int            *out_width,
                    int            *out_height)
{
  int width, height;

  width = gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (paintable));
  height = gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (paintable));
  
  if (desired > 0)
    {
      if (width > height)
        {
          height = (height * desired + width - 1) / width;
          width = desired;
        }
      else
        {
          width = (width * desired + width - 1) / height;
          height = desired;
        }
    }

  *out_width = width;
  *out_height = height;
}

static int
do_image (int         argc,
          const char *argv[],
          gboolean    do_node)
{
  OttieCreation *ottie;
  OttiePaintable *paintable;
  int width, height;
  int result = 0;
  double timestamp = 0;
  int desired_size = -1;

  while (TRUE)
    {
      if (argc == 0)
        return usage();

      if (strncmp (argv[0], "--time=", strlen ("--time=")) == 0)
        {
          timestamp = atof (argv[0] + strlen ("--time="));
        }
      else if (strncmp (argv[0], "--size=", strlen ("--size=")) == 0)
        {
          desired_size = atoi (argv[0] + strlen ("--size="));
        }
      else
        break;

      argc--;
      argv++;
    }
  
  if (argc != 2)
    return usage();

  ottie = ottie_creation_new_for_filename (argv[0]);
  if (ottie == NULL)
    {
      g_printerr ("Someone figure out error handling for loading ottie files.\n");
      return 1;
    }
  while (ottie_creation_is_loading (ottie))
    g_main_context_iteration (NULL, TRUE);

  paintable = ottie_paintable_new (ottie);
  ottie_paintable_set_timestamp (paintable, round (timestamp * G_USEC_PER_SEC));

  paintable_get_size (paintable, desired_size, &width, &height);

  if (do_node)
    {
      if (!save_paintable_to_node (GDK_PAINTABLE (paintable), argv[1], width, height))
        result = 1;
    }
  else
    {
      if (!save_paintable_to_png (GDK_PAINTABLE (paintable), argv[1], width, height))
        result = 1;
    }

  g_object_unref (paintable);

  return result;
}

static int
do_video (int         argc,
          const char *argv[])
{
  OttieCreation *ottie;
  OttiePaintable *paintable;
  int width, height;
  int result = 0;
  double timestamp;
  int desired_size = -1;
  GSubprocess *process;
  GError *error = NULL;
  GOutputStream *pipe;
  cairo_surface_t *surface;
  char *width_string, *height_string;
  char *location_string;

  while (TRUE)
    {
      if (argc == 0)
        return usage();

      if (strncmp (argv[0], "--size=", strlen ("--size=")) == 0)
        {
          desired_size = atoi (argv[0] + strlen ("--size="));
        }
      else
        break;

      argc--;
      argv++;
    }

  if (argc != 2)
    return usage();

  ottie = ottie_creation_new_for_filename (argv[0]);
  if (ottie == NULL)
    {
      g_printerr ("Someone figure out error handling for loading ottie files.\n");
      return 1;
    }
  while (ottie_creation_is_loading (ottie))
    g_main_context_iteration (NULL, TRUE);

  paintable = ottie_paintable_new (ottie);
  paintable_get_size (paintable, desired_size, &width, &height);

  width_string = g_strdup_printf ("width=%d", width);
  height_string = g_strdup_printf ("height=%d", height);
  location_string = g_strdup_printf ("location=%s", argv[1]);
  process = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE,
                              &error,
                              "gst-launch-1.0",
                              "fdsrc", 
                              "!", "rawvideoparse", "use-sink-caps=false", width_string, height_string,
                              G_BYTE_ORDER == G_LITTLE_ENDIAN ? "format=bgra" : "format=argb",
                              "!", "videoconvert",
                              "!", "vp9enc",
                              "!", "webmmux",
                              "!", "filesink", location_string,
                              NULL);
  g_free (width_string);
  g_free (height_string);
  g_free (location_string);

  if (process == NULL)
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      g_object_unref (paintable);
      return 1;
    }

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  pipe = g_subprocess_get_stdin_pipe (process);

  for (timestamp = 0; timestamp <= ottie_paintable_get_duration (paintable); timestamp += G_USEC_PER_SEC / 25)
    {
      ottie_paintable_set_timestamp (paintable, timestamp);
      draw_paintable (GDK_PAINTABLE (paintable), surface);
      if (!g_output_stream_write_all (pipe,
                                      cairo_image_surface_get_data (surface),
                                      width * height * 4,
                                      NULL,
                                      NULL,
                                      &error))
        {
          g_printerr ("%s\n", error->message);
          g_clear_error (&error);
          result = 1;
          break;
        }
    }

  if (!g_output_stream_close (pipe, NULL, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      result = 1;
    }
  if (!g_subprocess_wait (process, NULL, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      result = 1;
    }
  else if (!g_subprocess_get_successful (process))
    {
      g_printerr ("Encoder failed to write video.\n");
      result = 1;
    }

  g_object_unref (process);
  g_object_unref (paintable);

  return result;
}

static int
do_show (int         argc,
         const char *argv[])
{
  OttiePlayer *player;
  GtkWidget *video, *window;

  if (argc != 1)
    return usage();

  player = ottie_player_new_for_filename (argv[0]);
  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), argv[0]);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_window_destroy), NULL);

  video = gtk_video_new ();
  gtk_video_set_loop (GTK_VIDEO (video), TRUE);
  gtk_video_set_autoplay (GTK_VIDEO (video), TRUE);
  gtk_video_set_media_stream (GTK_VIDEO (video), GTK_MEDIA_STREAM (player));
  gtk_window_set_child (GTK_WINDOW (window), video);

  gtk_widget_show (window);

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}

int
main (int         argc,
      const char *argv[])
{
  int result;

  g_set_prgname ("ottie");

  gtk_init ();

  if (argc < 3)
    return usage ();

  if (strcmp (argv[2], "--help") == 0)
    return usage ();

  argv++;
  argc--;

  if (strcmp (argv[0], "image") == 0)
    result = do_image (argc - 1, argv + 1, FALSE);
  else if (strcmp (argv[0], "node") == 0)
    result = do_image (argc - 1, argv + 1, TRUE);
  else if (strcmp (argv[0], "video") == 0)
    result = do_video (argc - 1, argv + 1);
  else if (strcmp (argv[0], "view") == 0 ||
           strcmp (argv[0], "show") == 0)
    result = do_show (argc - 1, argv + 1);
  else
    return usage ();

  return result;
}
