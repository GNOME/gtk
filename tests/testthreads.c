/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdio.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "config.h"

#ifdef USE_PTHREADS
#include <pthread.h>

static int nthreads = 0;
static pthread_mutex_t nthreads_mutex = PTHREAD_MUTEX_INITIALIZER;

void
close_cb (GtkWidget *w, gint *flag)
{
  *flag = 1;
}

gint
delete_cb (GtkWidget *w, GdkEvent *event, gint *flag)
{
  *flag = 1;
  return TRUE;
}

void *
counter (void *data)
{
  gchar *name = data;
  gint flag = 0;
  gint counter = 0;
  gchar buffer[32];
  
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *button;

  gdk_threads_enter();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), name);
  gtk_widget_set_usize (window, 100, 50);

  vbox = gtk_vbox_new (FALSE, 0);

  g_signal_connect (window, "delete-event",
                    G_CALLBACK (delete_cb), &flag);

  gtk_container_add (GTK_CONTAINER (window), vbox);

  label = gtk_label_new ("0");
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

  button = gtk_button_new_with_label ("Close");
  g_signal_connect (button, "clicked",
                    G_CALLBACK (close_cb), &flag);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  gtk_widget_show_all (window);

  /* Since flag is only checked or set inside the GTK lock,
   * we don't have to worry about locking it explicitly
   */
  while (!flag)
    {
      sprintf(buffer, "%d", counter);
      gtk_label_set_text (GTK_LABEL (label), buffer);
      gdk_threads_leave();
      counter++;
      /* Give someone else a chance to get the lock next time.
       * Only necessary because we don't do anything else while
       * releasing the lock.
       */
      sleep(0);
      
      gdk_threads_enter();
    }

  gtk_widget_destroy (window);

  pthread_mutex_lock (&nthreads_mutex);
  nthreads--;
  if (nthreads == 0)
    gtk_main_quit();
  pthread_mutex_unlock (&nthreads_mutex);

  gdk_threads_leave();

  return NULL;
}

#endif

int 
main (int argc, char **argv)
{
#ifdef USE_PTHREADS
  int i;

  if (!gdk_threads_init())
    {
      fprintf(stderr, "Could not initialize threads\n");
      exit(1);
    }

  gtk_init (&argc, &argv);

  pthread_mutex_lock (&nthreads_mutex);

  for (i=0; i<5; i++)
    {
      char buffer[5][10];
      pthread_t thread;
      
      sprintf(buffer[i], "Thread %i", i);
      if (pthread_create (&thread, NULL, counter, buffer[i]))
	{
	  fprintf(stderr, "Couldn't create thread\n");
	  exit(1);
	}
      nthreads++;
    }

  pthread_mutex_unlock (&nthreads_mutex);

  gdk_threads_enter();
  gtk_main();
  gdk_threads_leave();
  fprintf(stderr, "Done\n");
#else /* !USE_PTHREADS */
  fprintf (stderr, "GTK+ not compiled with threads support\n");
  exit (1);
#endif /* USE_PTHREADS */  

  return 0;
}
