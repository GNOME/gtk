/* testclientmessage.c
 * Copyright (C) 2008  Novell, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>

static GdkAtom my_type;
static GdkAtom random_type;

static void
send_known (void)
{
  GdkEvent *event = gdk_event_new (GDK_CLIENT_EVENT);
  static int counter = 42;
  int i;
  
  event->client.window = NULL;
  event->client.message_type = my_type;
  event->client.data_format = 32;
  event->client.data.l[0] = counter++;
  for (i = 1; i < 5; i++)
    event->client.data.l[i] = 0;

  gdk_screen_broadcast_client_message (gdk_display_get_default_screen (gdk_display_get_default ()), event);
  
  gdk_event_free (event);
}

void
send_random (void)
{
  GdkEvent *event = gdk_event_new (GDK_CLIENT_EVENT);
  static int counter = 1;
  int i;
  
  event->client.window = NULL;
  event->client.message_type = random_type;
  event->client.data_format = 32;
  event->client.data.l[0] = counter++;
  for (i = 1; i < 5; i++)
    event->client.data.l[i] = 0;

  gdk_screen_broadcast_client_message (gdk_display_get_default_screen (gdk_display_get_default ()), event);
  
  gdk_event_free (event);
}

static GdkFilterReturn
filter_func (GdkXEvent *xevent,
	     GdkEvent  *event,
	     gpointer   data)
{
  g_print ("Got matching client message!\n");
  return GDK_FILTER_REMOVE;
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *button;

  gtk_init (&argc, &argv);

  my_type = gdk_atom_intern ("GtkTestClientMessage", FALSE);
  random_type = gdk_atom_intern (g_strdup_printf ("GtkTestClientMessage-%d",
						  g_rand_int_range (g_rand_new (), 1, 99)),
				 FALSE);

  g_print ("using random client message type %s\n", gdk_atom_name (random_type));

  window = g_object_connect (g_object_new (gtk_window_get_type (),
					   "type", GTK_WINDOW_TOPLEVEL,
					   "title", "testclientmessage",
					   "border_width", 10,
					   NULL),
			     "signal::destroy", gtk_main_quit, NULL,
			     NULL);
  vbox = g_object_new (gtk_vbox_get_type (),
		       "GtkWidget::parent", window,
		       "GtkWidget::visible", TRUE,
		       NULL);
  button = g_object_connect (g_object_new (gtk_button_get_type (),
					   "GtkButton::label", "send known client message",
					   "GtkWidget::parent", vbox,
					   "GtkWidget::visible", TRUE,
					   NULL),
			     "signal::clicked", send_known, NULL,
			     NULL);
  button = g_object_connect (g_object_new (gtk_button_get_type (),
					   "GtkButton::label", "send random client message",
					   "GtkWidget::parent", vbox,
					   "GtkWidget::visible", TRUE,
					   NULL),
			     "signal::clicked", send_random, NULL,
			     NULL);
  gdk_display_add_client_message_filter (gdk_display_get_default (),
					 my_type,
					 filter_func,
					 NULL);
  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
