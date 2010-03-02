
#define GTK_ENABLE_BROKEN
#include "config.h"
#include <stdio.h>
#include <gtk/gtk.h>

void text_toggle_editable (GtkWidget *checkbutton,
			   GtkWidget *text)
{
  gtk_text_set_editable (GTK_TEXT (text),
			 GTK_TOGGLE_BUTTON (checkbutton)->active);
}

void text_toggle_word_wrap (GtkWidget *checkbutton,
			    GtkWidget *text)
{
  gtk_text_set_word_wrap (GTK_TEXT (text),
			  GTK_TOGGLE_BUTTON (checkbutton)->active);
}

void close_application( GtkWidget *widget,
                        gpointer   data )
{
       gtk_main_quit ();
}

int main( int argc,
          char *argv[] )
{
  GtkWidget *window;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *check;
  GtkWidget *separator;
  GtkWidget *table;
  GtkWidget *vscrollbar;
  GtkWidget *text;
  GdkColormap *cmap;
  GdkColor color;
  GdkFont *fixed_font;

  FILE *infile;

  gtk_init (&argc, &argv);
 
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, 600, 500);
  gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);  
  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (close_application),
                    NULL);
  gtk_window_set_title (GTK_WINDOW (window), "Text Widget Example");
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);
  
  
  box1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), box1);
  gtk_widget_show (box1);
  
  
  box2 = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
  gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
  gtk_widget_show (box2);
  
  
  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacing (GTK_TABLE (table), 0, 2);
  gtk_table_set_col_spacing (GTK_TABLE (table), 0, 2);
  gtk_box_pack_start (GTK_BOX (box2), table, TRUE, TRUE, 0);
  gtk_widget_show (table);
  
  /* Create the GtkText widget */
  text = gtk_text_new (NULL, NULL);
  gtk_text_set_editable (GTK_TEXT (text), TRUE);
  gtk_table_attach (GTK_TABLE (table), text, 0, 1, 0, 1,
		    GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (text);

  /* Add a vertical scrollbar to the GtkText widget */
  vscrollbar = gtk_vscrollbar_new (GTK_TEXT (text)->vadj);
  gtk_table_attach (GTK_TABLE (table), vscrollbar, 1, 2, 0, 1,
		    GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (vscrollbar);

  /* Get the system color map and allocate the color red */
  cmap = gdk_colormap_get_system ();
  color.red = 0xffff;
  color.green = 0;
  color.blue = 0;
  if (!gdk_color_alloc (cmap, &color)) {
    g_error ("couldn't allocate color");
  }

  /* Load a fixed font */
  fixed_font = gdk_font_load ("-misc-fixed-medium-r-*-*-*-140-*-*-*-*-*-*");

  /* Realizing a widget creates a window for it,
   * ready for us to insert some text */
  gtk_widget_realize (text);

  /* Freeze the text widget, ready for multiple updates */
  gtk_text_freeze (GTK_TEXT (text));
  
  /* Insert some colored text */
  gtk_text_insert (GTK_TEXT (text), NULL, &text->style->black, NULL,
		   "Supports ", -1);
  gtk_text_insert (GTK_TEXT (text), NULL, &color, NULL,
		   "colored ", -1);
  gtk_text_insert (GTK_TEXT (text), NULL, &text->style->black, NULL,
		   "text and different ", -1);
  gtk_text_insert (GTK_TEXT (text), fixed_font, &text->style->black, NULL,
		   "fonts\n\n", -1);
  
  /* Load the file text.c into the text window */

  infile = fopen ("text.c", "r");
  
  if (infile) {
    char buffer[1024];
    int nchars;
    
    while (1)
      {
	nchars = fread (buffer, 1, 1024, infile);
	gtk_text_insert (GTK_TEXT (text), fixed_font, NULL,
			 NULL, buffer, nchars);
	
	if (nchars < 1024)
	  break;
      }
    
    fclose (infile);
  }

  /* Thaw the text widget, allowing the updates to become visible */  
  gtk_text_thaw (GTK_TEXT (text));
  
  hbox = gtk_hbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (box2), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  check = gtk_check_button_new_with_label ("Editable");
  gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (check), "toggled",
                    G_CALLBACK (text_toggle_editable), text);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);
  gtk_widget_show (check);
  check = gtk_check_button_new_with_label ("Wrap Words");
  gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (check), "toggled",
                    G_CALLBACK (text_toggle_word_wrap), text);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), FALSE);
  gtk_widget_show (check);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
  gtk_widget_show (separator);

  box2 = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
  gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
  gtk_widget_show (box2);
  
  button = gtk_button_new_with_label ("close");
  g_signal_connect (G_OBJECT (button), "clicked",
	            G_CALLBACK (close_application),
	            NULL);
  gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
  gtk_widget_set_can_default (button, TRUE);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);

  gtk_widget_show (window);

  gtk_main ();
  
  return 0;       
}
