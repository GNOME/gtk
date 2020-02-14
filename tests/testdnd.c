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
target_drag_leave (GtkDropTarget *dest,
                   GdkDrop       *drop,
                   GtkWidget     *widget)
{
  g_print("leave\n");
  have_drag = FALSE;
  gtk_image_set_from_pixbuf (GTK_IMAGE (widget), trashcan_closed);
}

gboolean
target_drag_motion (GtkDropTarget *dest,
                    GdkDrop       *drop,
                    int            x,
                    int            y,
                    GtkWidget     *widget) 
{
  char *s;

  if (!have_drag)
    {
      have_drag = TRUE;
      gtk_image_set_from_pixbuf (GTK_IMAGE (widget), trashcan_open);
    }

  s = gdk_content_formats_to_string (gdk_drop_get_formats (drop));
  g_print ("%s\n", s);

  gdk_drop_status (drop, GDK_ACTION_ALL);

  return TRUE;
}

static void
got_text_in_target (GObject *object,
                    GAsyncResult *result,
                    gpointer data)
{
  char *str;

  str = gdk_drop_read_text_finish (GDK_DROP (object), result, NULL);
  if (str)
    {
      g_print ("Received \"%s\" in target\n", str);
      g_free (str);
    }

  gdk_drop_finish (GDK_DROP (object), GDK_ACTION_MOVE);
}
 
gboolean
target_drag_drop (GtkDropTarget *dest,
                  GdkDrop       *drop,
                  int            x,
                  int            y,
                  GtkWidget     *widget)
{
  GdkContentFormats *formats;
  const char *format;

  g_print("drop\n");
  have_drag = FALSE;

  gtk_image_set_from_pixbuf (GTK_IMAGE (widget), trashcan_closed);

  formats = gdk_drop_get_formats (drop);
  format = gdk_content_formats_match_mime_type (formats, formats);
  if (format)
    {
      gdk_drop_read_text_async (drop, NULL, got_text_in_target, widget);
      return TRUE;
    }
  
  gdk_drop_status (drop, 0);

  return FALSE;
}

static GdkDragAction
action_make_unique (GdkDragAction action)
{
  if (gdk_drag_action_is_unique (action))
    return action;

  if (action & GDK_ACTION_COPY)
    return GDK_ACTION_COPY;

  if (action & GDK_ACTION_MOVE)
    return GDK_ACTION_MOVE;
  
  if (action & GDK_ACTION_LINK)
    return GDK_ACTION_LINK;
  
  g_assert_not_reached ();
  return 0;
}

static void
got_text (GObject *object,
          GAsyncResult *result,
          gpointer data)
{
  char *str;

  str = gdk_drop_read_text_finish (GDK_DROP (object), result, NULL);
  if (str)
    {
      g_print ("Received \"%s\" in label\n", str);
      g_free (str);
    }
}
 
void  
label_drag_drop (GtkDropTarget *dest,
                 GdkDrop       *drop,
                 int            x,
                 int            y,
                 GtkWidget     *widget)
{
  gdk_drop_read_text_async (drop, NULL, got_text, widget);
  GdkDragAction action = action_make_unique (gdk_drop_get_actions (drop));
  gdk_drop_finish (drop, action);
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
popup_motion (GtkDropTarget *dest,
              GdkDrop       *drop)
{
  gdk_drop_status (drop, GDK_ACTION_COPY);
  return TRUE;
}

void  
popup_enter (GtkDropTarget *dest)
{
g_print ("popup enter\n");
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
}

void  
popup_leave (GtkDropTarget *dest)
{
g_print ("popup leave\n");
  if (in_popup)
    {
      in_popup = FALSE;
      if (!popdown_timer)
	{
	  g_print ("added popdown\n");
	  popdown_timer = g_timeout_add (500, popdown_cb, NULL);
	}
    }
}

static gboolean
popup_drop (GtkDropTarget *dest,
            GdkDrop       *drop)
{
  gdk_drop_finish (drop, GDK_ACTION_COPY);
  popdown_cb (NULL);
  return TRUE;
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
	  
	  popup_window = gtk_window_new ();

	  grid = gtk_grid_new ();
          targets = gdk_content_formats_new_for_gtype (G_TYPE_STRING);

	  for (i=0; i<3; i++)
	    for (j=0; j<3; j++)
	      {
		char buffer[128];
                GtkDropTarget *dest;

		g_snprintf(buffer, sizeof(buffer), "%d,%d", i, j);
		button = gtk_button_new_with_label (buffer);
                gtk_widget_set_hexpand (button, TRUE);
                gtk_widget_set_vexpand (button, TRUE);
		gtk_grid_attach (GTK_GRID (grid), button, i, j, 1, 1);

                dest = gtk_drop_target_new (targets, GDK_ACTION_COPY | GDK_ACTION_MOVE);
		g_signal_connect (dest, "accept", G_CALLBACK (popup_motion), NULL);
		g_signal_connect (dest, "drag-enter", G_CALLBACK (popup_enter), NULL);
		g_signal_connect (dest, "drag-leave", G_CALLBACK (popup_leave), NULL);
		g_signal_connect (dest, "drag-drop", G_CALLBACK (popup_drop), NULL);
                gtk_widget_add_controller (button, GTK_EVENT_CONTROLLER (dest));
	      }
	  gtk_container_add (GTK_CONTAINER (popup_window), grid);
          gdk_content_formats_unref (targets);

	}
      gtk_widget_show (popup_window);
      popped_up = TRUE;
    }

  popup_timer = FALSE;

  return FALSE;
}

gboolean
popsite_motion (GtkDropTarget *dest,
                int            x,
                int            y,
                GtkWidget     *widget)
{
  return TRUE;
}

void  
popsite_enter (GtkDropTarget *dest)
{
g_print ("popsite enter\n");
  if (!popup_timer)
    popup_timer = g_timeout_add (500, popup_cb, NULL);

}

void  
popsite_leave (GtkDropTarget *dest)
{
g_print ("popsite leave\n");
  if (popup_timer)
    {
      g_source_remove (popup_timer);
      popup_timer = 0;
    }
}

void
source_drag_data_delete (GtkWidget *widget,
                         gpointer  data)
{
  g_print ("Delete the data!\n");
}
  
void
test_init (void)
{
  if (g_file_test ("../modules/input/immodules.cache", G_FILE_TEST_EXISTS))
    g_setenv ("GTK_IM_MODULE_FILE", "../modules/input/immodules.cache", TRUE);
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
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
  GdkTexture *texture;
  GdkContentProvider *content;
  GValue value = G_VALUE_INIT;
  GtkDragSource *source;
  GdkContentFormats *targets;
  GtkDropTarget *dest;
  gboolean done = FALSE;

  test_init ();
  
  gtk_init ();

  window = gtk_window_new ();
  g_signal_connect (window, "destroy",
		    G_CALLBACK (quit_cb), &done);

  
  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), grid);

  drag_icon = gdk_pixbuf_new_from_xpm_data (drag_icon_xpm);
  texture = gdk_texture_new_for_pixbuf (drag_icon);
  g_object_unref (drag_icon);
  trashcan_open = gdk_pixbuf_new_from_xpm_data (trashcan_open_xpm);
  trashcan_closed = gdk_pixbuf_new_from_xpm_data (trashcan_closed_xpm);
  
  label = gtk_label_new ("Drop Here\n");

  targets = gdk_content_formats_new (target_table, n_targets - 1); /* no rootwin */
  dest = gtk_drop_target_new (targets, GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (dest, "drag-drop", G_CALLBACK (label_drag_drop), NULL);
  gtk_widget_add_controller (label, GTK_EVENT_CONTROLLER (dest));

  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  label = gtk_label_new ("Popup\n");

  dest = gtk_drop_target_new (targets, GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (dest, "accept", G_CALLBACK (popsite_motion), NULL);
  g_signal_connect (dest, "drag-enter", G_CALLBACK (popsite_enter), NULL);
  g_signal_connect (dest, "drag-leave", G_CALLBACK (popsite_leave), NULL);
  gtk_widget_add_controller (label, GTK_EVENT_CONTROLLER (dest));

  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);

  gdk_content_formats_unref (targets);
  
  pixmap = gtk_image_new_from_pixbuf (trashcan_closed);
  targets = gdk_content_formats_new (NULL, 0);
  dest = gtk_drop_target_new (targets, 0);
  g_signal_connect (dest, "drag-leave", G_CALLBACK (target_drag_leave), pixmap);
  g_signal_connect (dest, "accept", G_CALLBACK (target_drag_motion), pixmap);
  g_signal_connect (dest, "drag-drop", G_CALLBACK (target_drag_drop), pixmap);
  gtk_widget_add_controller (pixmap, GTK_EVENT_CONTROLLER (dest));
  gdk_content_formats_unref (targets);

  gtk_widget_set_hexpand (pixmap, TRUE);
  gtk_widget_set_vexpand (pixmap, TRUE);
  gtk_grid_attach (GTK_GRID (grid), pixmap, 1, 0, 1, 1);


  /* Drag site */

  button = gtk_label_new ("Drag Here\n");

  source = gtk_drag_source_new ();
  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, "I'm data!");
  content = gdk_content_provider_new_for_value (&value);
  g_value_unset (&value);
  gtk_drag_source_set_content (source, content);
  g_object_unref (content);
  gtk_drag_source_set_actions (source, GDK_ACTION_COPY|GDK_ACTION_MOVE);
  gtk_widget_add_controller (button, GTK_EVENT_CONTROLLER (source));
  gtk_drag_source_set_icon (source, GDK_PAINTABLE (texture), 0, 0);

  g_object_unref (texture);

  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_vexpand (button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, 1, 1, 1);

  gtk_widget_show (window);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
