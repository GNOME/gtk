/*
 * Copyright (C) 2009 Carlos Garnacho  <carlosg@gnome.org>
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <gtk/gtk.h>
#include "testphotoalbumwidget.h"

static void
add_image (TestPhotoAlbumWidget *album,
           const gchar          *image_path)
{
  GdkPixbuf *pixbuf;

  pixbuf = gdk_pixbuf_new_from_file_at_size (image_path,
                                             200, -1,
                                             NULL);
  if (pixbuf)
    {
      test_photo_album_widget_add_image (album, pixbuf);
      g_object_unref (pixbuf);
    }
}

static void
read_directory (GFile                *directory,
                TestPhotoAlbumWidget *album)
{
  GFileEnumerator *enumerator;
  GError *error = NULL;
  GFileInfo *info;

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          &error);

  if (error)
    {
      g_warning ("%s\n", error->message);
      g_error_free (error);
      return;
    }

  while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL)
    {
      GFile *file;
      gchar *path;

      file = g_file_get_child (directory, g_file_info_get_name (info));

      path = g_file_get_path (file);
      add_image (album, path);
      g_free (path);

      g_object_unref (file);
    }

  g_file_enumerator_close (enumerator, NULL, NULL);
  g_object_unref (enumerator);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *widget;
  GFile *dir;

  if (argc != 2)
    {
      g_print ("USAGE: %s <path-to-directory-with-images>\n", argv[0]);
      return -1;
    }

  gtk_init (&argc, &argv);

  dir = g_file_new_for_commandline_arg (argv[1]);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  widget = test_photo_album_widget_new ();
  gtk_container_add (GTK_CONTAINER (window), widget);

  g_signal_connect (window, "delete-event",
                    G_CALLBACK (gtk_main_quit), NULL);

  read_directory (dir, TEST_PHOTO_ALBUM_WIDGET (widget));

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
