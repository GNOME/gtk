#include "gtk/gtk.h"

/* Target side drag signals */

/* XPM */
static char * drag_icon_xpm[] = {
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
static char * trashcan_closed_xpm[] = {
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
static char * trashcan_open_xpm[] = {
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

GdkPixmap *trashcan_open;
GdkPixmap *trashcan_open_mask;
GdkPixmap *trashcan_closed;
GdkPixmap *trashcan_closed_mask;

gboolean have_drag;

enum {
  TARGET_STRING,
  TARGET_ROOTWIN,
  TARGET_URL
};

static GtkTargetEntry target_table[] = {
  { "STRING",     0, TARGET_STRING },
  { "text/plain", 0, TARGET_STRING },
  { "text/uri-list", 0, TARGET_URL },
  { "application/x-rootwin-drop", 0, TARGET_ROOTWIN }
};

static guint n_targets = sizeof(target_table) / sizeof(target_table[0]);

void  
target_drag_leave	   (GtkWidget	       *widget,
			    GdkDragContext     *context,
			    guint               time)
{
  g_print("leave\n");
  have_drag = FALSE;
  gtk_pixmap_set (GTK_PIXMAP (widget), trashcan_closed, trashcan_closed_mask);
}

gboolean
target_drag_motion	   (GtkWidget	       *widget,
			    GdkDragContext     *context,
			    gint                x,
			    gint                y,
			    guint               time)
{
  GtkWidget *source_widget;

  if (!have_drag)
    {
      have_drag = TRUE;
      gtk_pixmap_set (GTK_PIXMAP (widget), trashcan_open, trashcan_open_mask);
    }

  source_widget = gtk_drag_get_source_widget (context);
  g_print("motion, source %s\n", source_widget ?
	    gtk_type_name (GTK_OBJECT (source_widget)->klass->type) :
	    "unknown");

  gdk_drag_status (context, context->suggested_action, time);
  return TRUE;
}

gboolean
target_drag_drop	   (GtkWidget	       *widget,
			    GdkDragContext     *context,
			    gint                x,
			    gint                y,
			    guint               time)
{
  g_print("drop\n");
  have_drag = FALSE;

  gtk_pixmap_set (GTK_PIXMAP (widget), trashcan_closed, trashcan_closed_mask);

  if (context->targets)
    {
      gtk_drag_get_data (widget, context, 
			 GPOINTER_TO_INT (context->targets->data), 
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
			    GtkSelectionData   *data,
			    guint               info,
			    guint               time)
{
  if ((data->length >= 0) && (data->format == 8))
    {
      g_print ("Received \"%s\" in trashcan\n", (gchar *)data->data);
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
			    GtkSelectionData   *data,
			    guint               info,
			    guint               time)
{
  if ((data->length >= 0) && (data->format == 8))
    {
      g_print ("Received \"%s\" in label\n", (gchar *)data->data);
      gtk_drag_finish (context, TRUE, FALSE, time);
      return;
    }
  
  gtk_drag_finish (context, FALSE, FALSE, time);
}

void  
source_drag_data_get  (GtkWidget          *widget,
		       GdkDragContext     *context,
		       GtkSelectionData   *selection_data,
		       guint               info,
		       guint               time,
		       gpointer            data)
{
  if (info == TARGET_ROOTWIN)
    g_print ("I was dropped on the rootwin\n");
  else if (info == TARGET_URL)
    gtk_selection_data_set (selection_data,
			    selection_data->target,
			    8, "file:///home/otaylor/images/weave.png", 37);
  else
    gtk_selection_data_set (selection_data,
			    selection_data->target,
			    8, "I'm Data!", 9);
}
  
/* The following is a rather elaborate example demonstrating/testing
 * changing of the window heirarchy during a drag - in this case,
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
	  gtk_timeout_remove (popdown_timer);
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
	  popdown_timer = gtk_timeout_add (500, popdown_cb, NULL);
	}
    }
}

gint
popup_cb (gpointer data)
{
  if (!popped_up)
    {
      if (!popup_window)
	{
	  GtkWidget *button;
	  GtkWidget *table;
	  int i, j;
	  
	  popup_window = gtk_window_new (GTK_WINDOW_POPUP);
	  gtk_window_set_position (GTK_WINDOW (popup_window), GTK_WIN_POS_MOUSE);

	  table = gtk_table_new (3,3, FALSE);

	  for (i=0; i<3; i++)
	    for (j=0; j<3; j++)
	      {
		char buffer[128];
		g_snprintf(buffer, sizeof(buffer), "%d,%d", i, j);
		button = gtk_button_new_with_label (buffer);
		gtk_table_attach (GTK_TABLE (table), button, i, i+1, j, j+1,
				  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
				  0, 0);

		gtk_drag_dest_set (button,
				   GTK_DEST_DEFAULT_ALL,
				   target_table, n_targets - 1, /* no rootwin */
				   GDK_ACTION_COPY | GDK_ACTION_MOVE);
		gtk_signal_connect (GTK_OBJECT (button), "drag_motion",
				    GTK_SIGNAL_FUNC (popup_motion), NULL);
		gtk_signal_connect (GTK_OBJECT (button), "drag_leave",
				    GTK_SIGNAL_FUNC (popup_leave), NULL);
	      }

	  gtk_widget_show_all (table);
	  gtk_container_add (GTK_CONTAINER (popup_window), table);

	}
      gtk_widget_show (popup_window);
      popped_up = TRUE;
    }

  popdown_timer = gtk_timeout_add (500, popdown_cb, NULL);
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
    popup_timer = gtk_timeout_add (500, popup_cb, NULL);

  return TRUE;
}

void  
popsite_leave	   (GtkWidget	       *widget,
		    GdkDragContext     *context,
		    guint               time)
{
  if (popup_timer)
    {
      gtk_timeout_remove (popup_timer);
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
  
int 
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *pixmap;
  GtkWidget *button;
  GdkPixmap *drag_icon;
  GdkPixmap *drag_mask;

  gtk_init (&argc, &argv); 

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect (GTK_OBJECT(window), "destroy",
		      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

  
  table = gtk_table_new (2, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (window), table);

  drag_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL,
						     gtk_widget_get_colormap (window),
						     &drag_mask,
						     NULL, drag_icon_xpm);

  trashcan_open = gdk_pixmap_colormap_create_from_xpm_d (NULL,
							 gtk_widget_get_colormap (window),
							 &trashcan_open_mask,
							 NULL, trashcan_open_xpm);
  trashcan_closed = gdk_pixmap_colormap_create_from_xpm_d (NULL,
							   gtk_widget_get_colormap (window),
							   &trashcan_closed_mask,
							   NULL, trashcan_closed_xpm);
  
  label = gtk_label_new ("Drop Here\n");

  gtk_drag_dest_set (label,
		     GTK_DEST_DEFAULT_ALL,
		     target_table, n_targets - 1, /* no rootwin */
		     GDK_ACTION_COPY | GDK_ACTION_MOVE);

  gtk_signal_connect( GTK_OBJECT(label), "drag_data_received",
		      GTK_SIGNAL_FUNC( label_drag_data_received), NULL);

  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
		    0, 0);

  label = gtk_label_new ("Popup\n");

  gtk_drag_dest_set (label,
		     GTK_DEST_DEFAULT_ALL,
		     target_table, n_targets - 1, /* no rootwin */
		     GDK_ACTION_COPY | GDK_ACTION_MOVE);

  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 1, 2,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
		    0, 0);

  gtk_signal_connect (GTK_OBJECT (label), "drag_motion",
		      GTK_SIGNAL_FUNC (popsite_motion), NULL);
  gtk_signal_connect (GTK_OBJECT (label), "drag_leave",
		      GTK_SIGNAL_FUNC (popsite_leave), NULL);
  
  pixmap = gtk_pixmap_new (trashcan_closed, trashcan_closed_mask);
  gtk_drag_dest_set (pixmap, 0, NULL, 0, 0);
  gtk_table_attach (GTK_TABLE (table), pixmap, 1, 2, 0, 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
		    0, 0);

  gtk_signal_connect (GTK_OBJECT (pixmap), "drag_leave",
		      GTK_SIGNAL_FUNC (target_drag_leave), NULL);

  gtk_signal_connect (GTK_OBJECT (pixmap), "drag_motion",
		      GTK_SIGNAL_FUNC (target_drag_motion), NULL);

  gtk_signal_connect (GTK_OBJECT (pixmap), "drag_drop",
		      GTK_SIGNAL_FUNC (target_drag_drop), NULL);

  gtk_signal_connect (GTK_OBJECT (pixmap), "drag_data_received",
		      GTK_SIGNAL_FUNC (target_drag_data_received), NULL);

  /* Drag site */

  button = gtk_button_new_with_label ("Drag Here\n");

  gtk_drag_source_set (button, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
		       target_table, n_targets, 
		       GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_drag_source_set_icon (button, 
			    gtk_widget_get_colormap (window),
			    drag_icon, drag_mask);

  gdk_pixmap_unref (drag_icon);
  gdk_pixmap_unref (drag_mask);

  gtk_table_attach (GTK_TABLE (table), button, 0, 1, 1, 2,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
		    0, 0);

  gtk_signal_connect (GTK_OBJECT (button), "drag_data_get",
		      GTK_SIGNAL_FUNC (source_drag_data_get), NULL);
  gtk_signal_connect (GTK_OBJECT (button), "drag_data_delete",
		      GTK_SIGNAL_FUNC (source_drag_data_delete), NULL);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
