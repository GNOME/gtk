/* testdnd.c
 * Copyright (C) 1998  Red Hat, Inc.
 * Author: Owen Taylor
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
#include "gtk/gtk.h"

/* Target side drag signals */

/* XPM */
static const char * drag_icon_xpm[] = {
"36 48 9 1",
" 	c None",
".	c #020204",
"+	c #8F8F90",
"@	c #D3D3D2",
"#	c #AEAEAC",
"$	c #ECECEC",
"%	c #A2A2A4",
"&	c #FEFEFC",
"*	c #BEBEBC",
"               .....................",
"              ..&&&&&&&&&&&&&&&&&&&.",
"             ...&&&&&&&&&&&&&&&&&&&.",
"            ..&.&&&&&&&&&&&&&&&&&&&.",
"           ..&&.&&&&&&&&&&&&&&&&&&&.",
"          ..&&&.&&&&&&&&&&&&&&&&&&&.",
"         ..&&&&.&&&&&&&&&&&&&&&&&&&.",
"        ..&&&&&.&&&@&&&&&&&&&&&&&&&.",
"       ..&&&&&&.*$%$+$&&&&&&&&&&&&&.",
"      ..&&&&&&&.%$%$+&&&&&&&&&&&&&&.",
"     ..&&&&&&&&.#&#@$&&&&&&&&&&&&&&.",
"    ..&&&&&&&&&.#$**#$&&&&&&&&&&&&&.",
"   ..&&&&&&&&&&.&@%&%$&&&&&&&&&&&&&.",
"  ..&&&&&&&&&&&.&&&&&&&&&&&&&&&&&&&.",
" ..&&&&&&&&&&&&.&&&&&&&&&&&&&&&&&&&.",
"................&$@&&&@&&&&&&&&&&&&.",
".&&&&&&&+&&#@%#+@#@*$%$+$&&&&&&&&&&.",
".&&&&&&&+&&#@#@&&@*%$%$+&&&&&&&&&&&.",
".&&&&&&&+&$%&#@&#@@#&#@$&&&&&&&&&&&.",
".&&&&&&@#@@$&*@&@#@#$**#$&&&&&&&&&&.",
".&&&&&&&&&&&&&&&&&&&@%&%$&&&&&&&&&&.",
".&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&.",
".&&&&&&&&$#@@$&&&&&&&&&&&&&&&&&&&&&.",
".&&&&&&&&&+&$+&$&@&$@&&$@&&&&&&&&&&.",
".&&&&&&&&&+&&#@%#+@#@*$%&+$&&&&&&&&.",
".&&&&&&&&&+&&#@#@&&@*%$%$+&&&&&&&&&.",
".&&&&&&&&&+&$%&#@&#@@#&#@$&&&&&&&&&.",
".&&&&&&&&@#@@$&*@&@#@#$#*#$&&&&&&&&.",
".&&&&&&&&&&&&&&&&&&&&&$%&%$&&&&&&&&.",
".&&&&&&&&&&$#@@$&&&&&&&&&&&&&&&&&&&.",
".&&&&&&&&&&&+&$%&$$@&$@&&$@&&&&&&&&.",
".&&&&&&&&&&&+&&#@%#+@#@*$%$+$&&&&&&.",
".&&&&&&&&&&&+&&#@#@&&@*#$%$+&&&&&&&.",
".&&&&&&&&&&&+&$+&*@&#@@#&#@$&&&&&&&.",
".&&&&&&&&&&$%@@&&*@&@#@#$#*#&&&&&&&.",
".&&&&&&&&&&&&&&&&&&&&&&&$%&%$&&&&&&.",
".&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&.",
".&&&&&&&&&&&&&&$#@@$&&&&&&&&&&&&&&&.",
".&&&&&&&&&&&&&&&+&$%&$$@&$@&&$@&&&&.",
".&&&&&&&&&&&&&&&+&&#@%#+@#@*$%$+$&&.",
".&&&&&&&&&&&&&&&+&&#@#@&&@*#$%$+&&&.",
".&&&&&&&&&&&&&&&+&$+&*@&#@@#&#@$&&&.",
".&&&&&&&&&&&&&&$%@@&&*@&@#@#$#*#&&&.",
".&&&&&&&&&&&&&&&&&&&&&&&&&&&$%&%$&&.",
".&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&.",
".&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&.",
".&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&.",
"...................................."};

/* XPM */
static const char * trashcan_closed_xpm[] = {
"64 80 17 1",
" 	c None",
".	c #030304",
"+	c #5A5A5C",
"@	c #323231",
"#	c #888888",
"$	c #1E1E1F",
"%	c #767677",
"&	c #494949",
"*	c #9E9E9C",
"=	c #111111",
"-	c #3C3C3D",
";	c #6B6B6B",
">	c #949494",
",	c #282828",
"'	c #808080",
")	c #545454",
"!	c #AEAEAC",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                       ==......=$$...===                        ",
"                 ..$------)+++++++++++++@$$...                  ",
"             ..=@@-------&+++++++++++++++++++-....              ",
"          =.$$@@@-&&)++++)-,$$$$=@@&+++++++++++++,..$           ",
"         .$$$$@@&+++++++&$$$@@@@-&,$,-++++++++++;;;&..          ",
"        $$$$,@--&++++++&$$)++++++++-,$&++++++;%%'%%;;$@         ",
"       .-@@-@-&++++++++-@++++++++++++,-++++++;''%;;;%*-$        ",
"       +------++++++++++++++++++++++++++++++;;%%%;;##*!.        ",
"        =+----+++++++++++++++++++++++;;;;;;;;;;;;%'>>).         ",
"         .=)&+++++++++++++++++;;;;;;;;;;;;;;%''>>#>#@.          ",
"          =..=&++++++++++++;;;;;;;;;;;;;%###>>###+%==           ",
"           .&....=-+++++%;;####''''''''''##'%%%)..#.            ",
"           .+-++@....=,+%#####'%%%%%%%%%;@$-@-@*++!.            ",
"           .+-++-+++-&-@$$=$=......$,,,@;&)+!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           =+-++-+++-+++++++++!++++!++++!+++!++!+++=            ",
"            $.++-+++-+++++++++!++++!++++!+++!++!+.$             ",
"              =.++++++++++++++!++++!++++!+++!++.=               ",
"                 $..+++++++++++++++!++++++...$                  ",
"                      $$=.............=$$                       ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                "};

/* XPM */
static const char * trashcan_open_xpm[] = {
"64 80 17 1",
" 	c None",
".	c #030304",
"+	c #5A5A5C",
"@	c #323231",
"#	c #888888",
"$	c #1E1E1F",
"%	c #767677",
"&	c #494949",
"*	c #9E9E9C",
"=	c #111111",
"-	c #3C3C3D",
";	c #6B6B6B",
">	c #949494",
",	c #282828",
"'	c #808080",
")	c #545454",
"!	c #AEAEAC",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                      .=.==.,@                  ",
"                                   ==.,@-&&&)-=                 ",
"                                 .$@,&++;;;%>*-                 ",
"                               $,-+)+++%%;;'#+.                 ",
"                            =---+++++;%%%;%##@.                 ",
"                           @)++++++++;%%%%'#%$                  ",
"                         $&++++++++++;%%;%##@=                  ",
"                       ,-++++)+++++++;;;'#%)                    ",
"                      @+++&&--&)++++;;%'#'-.                    ",
"                    ,&++-@@,,,,-)++;;;'>'+,                     ",
"                  =-++&@$@&&&&-&+;;;%##%+@                      ",
"                =,)+)-,@@&+++++;;;;%##%&@                       ",
"               @--&&,,@&)++++++;;;;'#)@                         ",
"              ---&)-,@)+++++++;;;%''+,                          ",
"            $--&)+&$-+++++++;;;%%'';-                           ",
"           .,-&+++-$&++++++;;;%''%&=                            ",
"          $,-&)++)-@++++++;;%''%),                              ",
"         =,@&)++++&&+++++;%'''+$@&++++++                        ",
"        .$@-++++++++++++;'#';,........=$@&++++                  ",
"       =$@@&)+++++++++++'##-.................=&++               ",
"      .$$@-&)+++++++++;%#+$.....................=)+             ",
"      $$,@-)+++++++++;%;@=........................,+            ",
"     .$$@@-++++++++)-)@=............................            ",
"     $,@---)++++&)@===............................,.            ",
"    $-@---&)))-$$=..............................=)!.            ",
"     --&-&&,,$=,==...........................=&+++!.            ",
"      =,=$..=$+)+++++&@$=.............=$@&+++++!++!.            ",
"           .)-++-+++++++++++++++++++++++++++!++!++!.            ",
"           .+-++-+++++++++++++++++++++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!+++!!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           .+-++-+++-+++++++++!++++!++++!+++!++!++!.            ",
"           =+-++-+++-+++++++++!++++!++++!+++!++!+++=            ",
"            $.++-+++-+++++++++!++++!++++!+++!++!+.$             ",
"              =.++++++++++++++!++++!++++!+++!++.=               ",
"                 $..+++++++++++++++!++++++...$                  ",
"                      $$==...........==$$                       ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                ",
"                                                                "};

GdkPixbuf *trashcan_open;
GdkPixbuf *trashcan_closed;

gboolean have_drag;

static const char *target_table[] = {
  "STRING",
  "text/plain",
  "application/x-rootwindow-drop"
};

static guint n_targets = sizeof(target_table) / sizeof(target_table[0]);

void  
target_drag_leave	   (GtkWidget	       *widget,
			    GdkDragContext     *context,
			    guint               time)
{
  g_print("leave\n");
  have_drag = FALSE;
  gtk_image_set_from_pixbuf (GTK_IMAGE (widget), trashcan_closed);
}

gboolean
target_drag_motion	   (GtkWidget	       *widget,
			    GdkDragContext     *context,
			    gint                x,
			    gint                y,
			    guint               time)
{
  GtkWidget *source_widget;
  char *s;

  if (!have_drag)
    {
      have_drag = TRUE;
      gtk_image_set_from_pixbuf (GTK_IMAGE (widget), trashcan_open);
    }

  source_widget = gtk_drag_get_source_widget (context);
  g_print ("motion, source %s\n", source_widget ?
	   G_OBJECT_TYPE_NAME (source_widget) :
	   "NULL");

  s = gdk_content_formats_to_string (gdk_drag_context_get_formats (context));
  g_print ("%s\n", s);

  gdk_drag_status (context, gdk_drag_context_get_suggested_action (context), time);

  return TRUE;
}

gboolean
target_drag_drop	   (GtkWidget	       *widget,
			    GdkDragContext     *context,
			    gint                x,
			    gint                y,
			    guint               time)
{
  GdkContentFormats *formats;
  GdkAtom format;

  g_print("drop\n");
  have_drag = FALSE;

  gtk_image_set_from_pixbuf (GTK_IMAGE (widget), trashcan_closed);

  formats = gdk_drag_context_get_formats (context);
  gdk_content_formats_match (formats, formats, NULL, &format);
  if (format)
    {
      gtk_drag_get_data (widget, context,
			 format,
			 time);
      return TRUE;
    }
  
  return FALSE;
}

void  
target_drag_data_received  (GtkWidget          *widget,
			    GdkDragContext     *context,
			    gint                x,
			    gint                y,
			    GtkSelectionData   *selection_data,
			    guint               info,
			    guint               time)
{
  if (gtk_selection_data_get_length (selection_data) >= 0 &&
      gtk_selection_data_get_format (selection_data) == 8)
    {
      g_print ("Received \"%s\" in trashcan\n", (gchar *) gtk_selection_data_get_data (selection_data));
      gtk_drag_finish (context, TRUE, FALSE, time);
      return;
    }
  
  gtk_drag_finish (context, FALSE, FALSE, time);
}
  
void  
label_drag_data_received  (GtkWidget          *widget,
			    GdkDragContext     *context,
			    gint                x,
			    gint                y,
			    GtkSelectionData   *selection_data,
			    guint               info,
			    guint               time)
{
  if (gtk_selection_data_get_length (selection_data) >= 0 &&
      gtk_selection_data_get_format (selection_data) == 8)
    {
      g_print ("Received \"%s\" in label\n", (gchar *) gtk_selection_data_get_data (selection_data));
      gtk_drag_finish (context, TRUE, FALSE, time);
      return;
    }
  
  gtk_drag_finish (context, FALSE, FALSE, time);
}

void  
source_drag_data_get  (GtkWidget          *widget,
		       GdkDragContext     *context,
		       GtkSelectionData   *selection_data,
		       guint               time,
		       gpointer            data)
{
  if (gtk_selection_data_get_target (selection_data) == gdk_atom_intern_static_string ("application/x-rootwindow-drop"))
    g_print ("I was dropped on the rootwin\n");
  else
    gtk_selection_data_set (selection_data,
			    gtk_selection_data_get_target (selection_data),
			    8, (guchar *) "I'm Data!", 9);
}
  
/* The following is a rather elaborate example demonstrating/testing
 * changing of the window hierarchy during a drag - in this case,
 * via a "spring-loaded" popup window.
 */
static GtkWidget *popup_window = NULL;

static gboolean popped_up = FALSE;
static gboolean in_popup = FALSE;
static guint popdown_timer = 0;
static guint popup_timer = 0;

gint
popdown_cb (gpointer data)
{
  popdown_timer = 0;

  gtk_widget_hide (popup_window);
  popped_up = FALSE;

  return FALSE;
}

gboolean
popup_motion	   (GtkWidget	       *widget,
		    GdkDragContext     *context,
		    gint                x,
		    gint                y,
		    guint               time)
{
  if (!in_popup)
    {
      in_popup = TRUE;
      if (popdown_timer)
	{
	  g_print ("removed popdown\n");
	  g_source_remove (popdown_timer);
	  popdown_timer = 0;
	}
    }

  return TRUE;
}

void  
popup_leave	   (GtkWidget	       *widget,
		    GdkDragContext     *context,
		    guint               time)
{
  if (in_popup)
    {
      in_popup = FALSE;
      if (!popdown_timer)
	{
	  g_print ("added popdown\n");
	  popdown_timer = gdk_threads_add_timeout (500, popdown_cb, NULL);
	}
    }
}

gboolean
popup_cb (gpointer data)
{
  if (!popped_up)
    {
      if (!popup_window)
	{
	  GtkWidget *button;
	  GtkWidget *grid;
	  int i, j;
          GdkContentFormats *targets;
	  
	  popup_window = gtk_window_new (GTK_WINDOW_POPUP);
	  gtk_window_set_position (GTK_WINDOW (popup_window), GTK_WIN_POS_MOUSE);

	  grid = gtk_grid_new ();
          targets = gdk_content_formats_new (target_table, n_targets - 1); /* no rootwin */

	  for (i=0; i<3; i++)
	    for (j=0; j<3; j++)
	      {
		char buffer[128];
		g_snprintf(buffer, sizeof(buffer), "%d,%d", i, j);
		button = gtk_button_new_with_label (buffer);
                gtk_widget_set_hexpand (button, TRUE);
                gtk_widget_set_vexpand (button, TRUE);
		gtk_grid_attach (GTK_GRID (grid), button, i, j, 1, 1);

		gtk_drag_dest_set (button,
				   GTK_DEST_DEFAULT_ALL,
                                   targets,
				   GDK_ACTION_COPY | GDK_ACTION_MOVE);
		g_signal_connect (button, "drag_motion",
				  G_CALLBACK (popup_motion), NULL);
		g_signal_connect (button, "drag_leave",
				  G_CALLBACK (popup_leave), NULL);
	      }
	  gtk_container_add (GTK_CONTAINER (popup_window), grid);
          gdk_content_formats_unref (targets);

	}
      gtk_widget_show (popup_window);
      popped_up = TRUE;
    }

  popdown_timer = gdk_threads_add_timeout (500, popdown_cb, NULL);
  g_print ("added popdown\n");

  popup_timer = FALSE;

  return FALSE;
}

gboolean
popsite_motion	   (GtkWidget	       *widget,
		    GdkDragContext     *context,
		    gint                x,
		    gint                y,
		    guint               time)
{
  if (!popup_timer)
    popup_timer = gdk_threads_add_timeout (500, popup_cb, NULL);

  return TRUE;
}

void  
popsite_leave	   (GtkWidget	       *widget,
		    GdkDragContext     *context,
		    guint               time)
{
  if (popup_timer)
    {
      g_source_remove (popup_timer);
      popup_timer = 0;
    }
}

void  
source_drag_data_delete  (GtkWidget          *widget,
			  GdkDragContext     *context,
			  gpointer            data)
{
  g_print ("Delete the data!\n");
}
  
void
test_init (void)
{
  if (g_file_test ("../modules/input/immodules.cache", G_FILE_TEST_EXISTS))
    g_setenv ("GTK_IM_MODULE_FILE", "../modules/input/immodules.cache", TRUE);
}

int 
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *pixmap;
  GtkWidget *button;
  GdkPixbuf *drag_icon;
  cairo_surface_t *surface;
  GdkContentFormats *targets;

  test_init ();
  
  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);

  
  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), grid);

  drag_icon = gdk_pixbuf_new_from_xpm_data (drag_icon_xpm);
  surface = gdk_cairo_surface_create_from_pixbuf (drag_icon, 1, NULL);
  g_object_unref (drag_icon);
  trashcan_open = gdk_pixbuf_new_from_xpm_data (trashcan_open_xpm);
  trashcan_closed = gdk_pixbuf_new_from_xpm_data (trashcan_closed_xpm);
  
  label = gtk_label_new ("Drop Here\n");

  targets = gdk_content_formats_new (target_table, n_targets - 1); /* no rootwin */
  gtk_drag_dest_set (label,
		     GTK_DEST_DEFAULT_ALL,
                     targets,
		     GDK_ACTION_COPY | GDK_ACTION_MOVE);

  g_signal_connect (label, "drag_data_received",
		    G_CALLBACK( label_drag_data_received), NULL);

  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  label = gtk_label_new ("Popup\n");

  gtk_drag_dest_set (label,
		     GTK_DEST_DEFAULT_ALL,
                     targets,
		     GDK_ACTION_COPY | GDK_ACTION_MOVE);

  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);

  g_signal_connect (label, "drag_motion",
		    G_CALLBACK (popsite_motion), NULL);
  g_signal_connect (label, "drag_leave",
		    G_CALLBACK (popsite_leave), NULL);
  gdk_content_formats_unref (targets);
  
  pixmap = gtk_image_new_from_pixbuf (trashcan_closed);
  gtk_drag_dest_set (pixmap, 0, NULL, 0);
  gtk_widget_set_hexpand (pixmap, TRUE);
  gtk_widget_set_vexpand (pixmap, TRUE);
  gtk_grid_attach (GTK_GRID (grid), pixmap, 1, 0, 1, 1);

  g_signal_connect (pixmap, "drag_leave",
		    G_CALLBACK (target_drag_leave), NULL);

  g_signal_connect (pixmap, "drag_motion",
		    G_CALLBACK (target_drag_motion), NULL);

  g_signal_connect (pixmap, "drag_drop",
		    G_CALLBACK (target_drag_drop), NULL);

  g_signal_connect (pixmap, "drag_data_received",
		    G_CALLBACK (target_drag_data_received), NULL);

  /* Drag site */

  button = gtk_button_new_with_label ("Drag Here\n");

  targets = gdk_content_formats_new (target_table, n_targets);
  gtk_drag_source_set (button, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                       targets,
		       GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_drag_source_set_icon_surface (button, surface);
  gdk_content_formats_unref (targets);

  cairo_surface_destroy (surface);

  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_vexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, 1, 1, 1);

  g_signal_connect (button, "drag_data_get",
		    G_CALLBACK (source_drag_data_get), NULL);
  g_signal_connect (button, "drag_data_delete",
		    G_CALLBACK (source_drag_data_delete), NULL);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
