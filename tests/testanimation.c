
/* testpixbuf -- test program for gdk-pixbuf code
 * Copyright (C) 1999 Mark Crichton, Larry Ewing
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <gtk/gtk.h>

typedef struct _LoadContext LoadContext;

struct _LoadContext
{
  gchar *filename;
  GtkWidget *window;
  GdkPixbufLoader *pixbuf_loader;
  guint load_timeout;
  FILE* image_stream;
};

static void
destroy_context (gpointer data)
{
  LoadContext *lc = data;

  g_free (lc->filename);
  
  if (lc->load_timeout)
    g_source_remove (lc->load_timeout);

  if (lc->image_stream)
    fclose (lc->image_stream);

  if (lc->pixbuf_loader)
    {
      gdk_pixbuf_loader_close (lc->pixbuf_loader, NULL);
      g_object_unref (lc->pixbuf_loader);
    }
  
  g_free (lc);
}

static LoadContext*
get_load_context (GtkWidget *image)
{
  LoadContext *lc;

  lc = g_object_get_data (G_OBJECT (image), "lc");

  if (lc == NULL)
    {
      lc = g_new0 (LoadContext, 1);

      g_object_set_data_full (G_OBJECT (image),        
                              "lc",
                              lc,
                              destroy_context);
    }

  return lc;
}

static void
progressive_prepared_callback (GdkPixbufLoader* loader,
                               gpointer         data)
{
  GdkPixbuf* pixbuf;
  GtkWidget* image;

  image = GTK_WIDGET (data);
    
  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

  /* Avoid displaying random memory contents, since the pixbuf
   * isn't filled in yet.
   */
  gdk_pixbuf_fill (pixbuf, 0xaaaaaaff);

  /* Could set the pixbuf instead, if we only wanted to display
   * static images.
   */
  gtk_image_set_from_animation (GTK_IMAGE (image),
                                gdk_pixbuf_loader_get_animation (loader));
}

static void
progressive_updated_callback (GdkPixbufLoader* loader,
                              gint x, gint y, gint width, gint height,
                              gpointer data)
{
  GtkWidget* image;
  
  image = GTK_WIDGET (data);

  /* We know the pixbuf inside the GtkImage has changed, but the image
   * itself doesn't know this; so queue a redraw.  If we wanted to be
   * really efficient, we could use a drawing area or something
   * instead of a GtkImage, so we could control the exact position of
   * the pixbuf on the display, then we could queue a draw for only
   * the updated area of the image.
   */

  /* We only really need to redraw if the image's animation iterator
   * is gdk_pixbuf_animation_iter_on_currently_loading_frame(), but
   * who cares.
   */
  
  gtk_widget_queue_draw (image);
}

static gint
progressive_timeout (gpointer data)
{
  GtkWidget *image;
  LoadContext *lc;
  
  image = GTK_WIDGET (data);
  lc = get_load_context (image);
  
  /* This shows off fully-paranoid error handling, so looks scary.
   * You could factor out the error handling code into a nice separate
   * function to make things nicer.
   */
  
  if (lc->image_stream)
    {
      size_t bytes_read;
      guchar buf[256];
      GError *error = NULL;
      
      bytes_read = fread (buf, 1, 256, lc->image_stream);

      if (ferror (lc->image_stream))
        {
          GtkWidget *dialog;
          
          dialog = gtk_message_dialog_new (GTK_WINDOW (lc->window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Failure reading image file 'alphatest.png': %s",
                                           g_strerror (errno));

          g_signal_connect (dialog, "response",
			    G_CALLBACK (gtk_widget_destroy), NULL);

          fclose (lc->image_stream);
          lc->image_stream = NULL;

          gtk_widget_show (dialog);
          
          lc->load_timeout = 0;

          return FALSE; /* uninstall the timeout */
        }

      if (!gdk_pixbuf_loader_write (lc->pixbuf_loader,
                                    buf, bytes_read,
                                    &error))
        {
          GtkWidget *dialog;
          
          dialog = gtk_message_dialog_new (GTK_WINDOW (lc->window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Failed to load image: %s",
                                           error->message);

          g_error_free (error);
          
          g_signal_connect (dialog, "response",
			    G_CALLBACK (gtk_widget_destroy), NULL);

          fclose (lc->image_stream);
          lc->image_stream = NULL;
          
          gtk_widget_show (dialog);

          lc->load_timeout = 0;

          return FALSE; /* uninstall the timeout */
        }

      if (feof (lc->image_stream))
        {
          fclose (lc->image_stream);
          lc->image_stream = NULL;

          /* Errors can happen on close, e.g. if the image
           * file was truncated we'll know on close that
           * it was incomplete.
           */
          error = NULL;
          if (!gdk_pixbuf_loader_close (lc->pixbuf_loader,
                                        &error))
            {
              GtkWidget *dialog;
              
              dialog = gtk_message_dialog_new (GTK_WINDOW (lc->window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "Failed to load image: %s",
                                               error->message);
              
              g_error_free (error);
              
              g_signal_connect (dialog, "response",
				G_CALLBACK (gtk_widget_destroy), NULL);
              
              gtk_widget_show (dialog);

              g_object_unref (lc->pixbuf_loader);
              lc->pixbuf_loader = NULL;
              
              lc->load_timeout = 0;
              
              return FALSE; /* uninstall the timeout */
            }
          
          g_object_unref (lc->pixbuf_loader);
          lc->pixbuf_loader = NULL;
        }
    }
  else
    {
      lc->image_stream = fopen (lc->filename, "r");

      if (lc->image_stream == NULL)
        {
          GtkWidget *dialog;
          
          dialog = gtk_message_dialog_new (GTK_WINDOW (lc->window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Unable to open image file '%s': %s",
                                           lc->filename,
                                           g_strerror (errno));

          g_signal_connect (dialog, "response",
			    G_CALLBACK (gtk_widget_destroy), NULL);
          
          gtk_widget_show (dialog);

          lc->load_timeout = 0;

          return FALSE; /* uninstall the timeout */
        }

      if (lc->pixbuf_loader)
        {
          gdk_pixbuf_loader_close (lc->pixbuf_loader, NULL);
          g_object_unref (lc->pixbuf_loader);
          lc->pixbuf_loader = NULL;
        }
      
      lc->pixbuf_loader = gdk_pixbuf_loader_new ();
      
      g_signal_connect (lc->pixbuf_loader, "area_prepared",
			G_CALLBACK (progressive_prepared_callback), image);
      g_signal_connect (lc->pixbuf_loader, "area_updated",
			G_CALLBACK (progressive_updated_callback), image);
    }

  /* leave timeout installed */
  return TRUE;
}

static void
start_progressive_loading (GtkWidget *image)
{
  LoadContext *lc;

  lc = get_load_context (image);
  
  /* This is obviously totally contrived (we slow down loading
   * on purpose to show how incremental loading works).
   * The real purpose of incremental loading is the case where
   * you are reading data from a slow source such as the network.
   * The timeout simply simulates a slow data source by inserting
   * pauses in the reading process.
   */
  lc->load_timeout = gdk_threads_add_timeout (100,
                                    progressive_timeout,
                                    image);
}

static GtkWidget *
do_image (const char *filename)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *align;
  GtkWidget *window;
  gchar *str, *escaped;
  LoadContext *lc;
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Image Loading");

  gtk_container_set_border_width (GTK_CONTAINER (window), 8);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  label = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  escaped = g_markup_escape_text (filename, -1);
  str = g_strdup_printf ("Progressively loading: <b>%s</b>", escaped);
  gtk_label_set_markup (GTK_LABEL (label),
                        str);
  g_free (escaped);
  g_free (str);
  
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
      
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  /* The alignment keeps the frame from growing when users resize
   * the window
   */
  align = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_container_add (GTK_CONTAINER (align), frame);
  gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);      

  image = gtk_image_new_from_pixbuf (NULL);
  gtk_container_add (GTK_CONTAINER (frame), image);

  lc = get_load_context (image);

  lc->window = window;
  lc->filename = g_strdup (filename);
  
  start_progressive_loading (image);

  g_signal_connect (window, "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);
  
  g_signal_connect (window, "delete_event",
		    G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show_all (window);

  return window;
}

static void
do_nonprogressive (const gchar *filename)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *align;
  GtkWidget *window;
  gchar *str, *escaped;
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Animation");

  gtk_container_set_border_width (GTK_CONTAINER (window), 8);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  label = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  escaped = g_markup_escape_text (filename, -1);
  str = g_strdup_printf ("Loaded from file: <b>%s</b>", escaped);
  gtk_label_set_markup (GTK_LABEL (label),
                        str);
  g_free (escaped);
  g_free (str);
  
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
      
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  /* The alignment keeps the frame from growing when users resize
   * the window
   */
  align = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_container_add (GTK_CONTAINER (align), frame);
  gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);      

  image = gtk_image_new_from_file (filename);
  gtk_container_add (GTK_CONTAINER (frame), image);

  g_signal_connect (window, "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);
  
  g_signal_connect (window, "delete_event",
		    G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show_all (window);
}

int
main (int    argc,
      char **argv)
{
  gint i;
  
  gtk_init (&argc, &argv);

  i = 1;
  while (i < argc)
    {
      do_image (argv[i]);
      do_nonprogressive (argv[i]);
      
      ++i;
    }

  gtk_main ();
  
  return 0;
}

