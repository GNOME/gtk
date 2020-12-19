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

static int
usage (void)
{
  g_print ("Usage:\n"
           "snapshot [OPTION…] TEST REFERENCE\n"
           "  Compare a snapshot of TEST to the REFERENCE.\n"
           "  --time=[timestamp]  Forward to [timestamp] seconds\n"
           "\n");

  return 1;
}
             
static GBytes *
diff_with_file (const char  *file1,
                GBytes      *input,
                GError     **error)
{
  GSubprocess *process;
  GBytes *output;

  process = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE
                              | G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                              error,
                              "diff", "-u", file1, "-", NULL);
  if (process == NULL)
    return NULL;

  if (!g_subprocess_communicate (process,
                                 input,
                                 NULL,
                                 &output,
                                 NULL,
                                 error))
    {
      g_object_unref (process);
      return NULL;
    }

  if (!g_subprocess_get_successful (process) &&
      /* this is the condition when the files differ */
      !(g_subprocess_get_if_exited (process) && g_subprocess_get_exit_status (process) == 1))
    {
      g_clear_pointer (&output, g_bytes_unref);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "The `diff' process exited with error status %d",
                   g_subprocess_get_exit_status (process));
    }

  g_object_unref (process);

  return output;
}

static gboolean
test (const char *testfile,
      const char *reffile,
      gint64      timestamp)
{
  OttieCreation *ottie;
  OttiePaintable *paintable;
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  GBytes *bytes, *diff;
  GError *error = NULL;

  ottie = ottie_creation_new_for_filename (testfile);
  if (ottie == NULL)
    {
      g_printerr ("Someone figure out error handling for loading ottie files.\n");
      return FALSE;
    }
  while (ottie_creation_is_loading (ottie))
    g_main_context_iteration (NULL, TRUE);

  paintable = ottie_paintable_new (ottie);
  ottie_paintable_set_timestamp (paintable, timestamp);

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (GDK_PAINTABLE (paintable),
                          snapshot,
                          ottie_creation_get_width (ottie),
                          ottie_creation_get_height (ottie));
  node = gtk_snapshot_free_to_node (snapshot);
  if (node != NULL)
    {
      bytes = gsk_render_node_serialize (node);
      gsk_render_node_unref (node);
    }
  else
    bytes = g_bytes_new_static ("", 0);

  diff = diff_with_file (reffile, bytes, &error);

  g_bytes_unref (bytes);
  g_object_unref (paintable);

  if (diff == NULL)
    {
      g_printerr ("Error diffing: %s\n", error->message);
      g_clear_error (&error);
      return FALSE;
    }

  if (diff && g_bytes_get_size (diff) > 0)
    {
      g_print ("Resulting file doesn't match reference:\n%s\n",
               (const char *) g_bytes_get_data (diff, NULL));
      g_bytes_unref (diff);
      return FALSE;
    }

  g_bytes_unref (diff);
  return TRUE;
}

int
main (int argc, char **argv)
{
  guint timestamp = 0;
  int result;

  gtk_test_init (&argc, &argv);

  argc--;
  argv++;

  while (TRUE)
    {
      if (argc == 0)
        return usage();

      if (strncmp (argv[0], "--time=", strlen ("--time=")) == 0)
        {
          timestamp = atof (argv[0] + strlen ("--time="));
        }
      else
        break;

      argc--;
      argv++;
    }
  
  if (argc % 2 != 0)
    return usage ();

  result = 0;
  for (int i = 0; i < argc / 2; i++)
    {
      if (!test (argv[2 * i],
                 argv[2 * i + 1],
                 round (timestamp * G_USEC_PER_SEC)))
        result++;
    }

  return result;
}

