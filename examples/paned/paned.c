/* example-start paned paned.c */

#include <gtk/gtk.h>
   
/* Create the list of "messages" */
GtkWidget *create_list( void )
{

    GtkWidget *scrolled_window;
    GtkWidget *list;
    GtkWidget *list_item;
   
    int i;
    char buffer[16];
   
    /* Create a new scrolled window, with scrollbars only if needed */
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				    GTK_POLICY_AUTOMATIC, 
				    GTK_POLICY_AUTOMATIC);
   
    /* Create a new list and put it in the scrolled window */
    list = gtk_list_new ();
    gtk_scrolled_window_add_with_viewport (
               GTK_SCROLLED_WINDOW (scrolled_window), list);
    gtk_widget_show (list);
   
    /* Add some messages to the window */
    for (i=0; i<10; i++) {

    	sprintf(buffer,"Message #%d",i);
    	list_item = gtk_list_item_new_with_label (buffer);
    	gtk_container_add (GTK_CONTAINER(list), list_item);
    	gtk_widget_show (list_item);

    }
   
    return scrolled_window;
}
   
/* Add some text to our text widget - this is a callback that is invoked
when our window is realized. We could also force our window to be
realized with gtk_widget_realize, but it would have to be part of
a hierarchy first */

void realize_text( GtkWidget *text,
                   gpointer data )
{
    gtk_text_freeze (GTK_TEXT (text));
    gtk_text_insert (GTK_TEXT (text), NULL, &text->style->black, NULL,
    "From: pathfinder@nasa.gov\n"
    "To: mom@nasa.gov\n"
    "Subject: Made it!\n"
    "\n"
    "We just got in this morning. The weather has been\n"
    "great - clear but cold, and there are lots of fun sights.\n"
    "Sojourner says hi. See you soon.\n"
    " -Path\n", -1);
   
    gtk_text_thaw (GTK_TEXT (text));
}
   
/* Create a scrolled text area that displays a "message" */
GtkWidget *create_text( void )
{
    GtkWidget *table;
    GtkWidget *text;
    GtkWidget *hscrollbar;
    GtkWidget *vscrollbar;
   
    /* Create a table to hold the text widget and scrollbars */
    table = gtk_table_new (2, 2, FALSE);
   
    /* Put a text widget in the upper left hand corner. Note the use of
     * GTK_SHRINK in the y direction */
    text = gtk_text_new (NULL, NULL);
    gtk_table_attach (GTK_TABLE (table), text, 0, 1, 0, 1,
		      GTK_FILL | GTK_EXPAND,
		      GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_widget_show (text);
   
    /* Put a HScrollbar in the lower left hand corner */
    hscrollbar = gtk_hscrollbar_new (GTK_TEXT (text)->hadj);
    gtk_table_attach (GTK_TABLE (table), hscrollbar, 0, 1, 1, 2,
		      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show (hscrollbar);
   
    /* And a VScrollbar in the upper right */
    vscrollbar = gtk_vscrollbar_new (GTK_TEXT (text)->vadj);
    gtk_table_attach (GTK_TABLE (table), vscrollbar, 1, 2, 0, 1,
		      GTK_FILL, GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show (vscrollbar);
   
    /* Add a handler to put a message in the text widget when it is realized */
    gtk_signal_connect (GTK_OBJECT (text), "realize",
			GTK_SIGNAL_FUNC (realize_text), NULL);
   
    return table;
}
   
int main( int   argc,
          char *argv[] )
{
    GtkWidget *window;
    GtkWidget *vpaned;
    GtkWidget *list;
    GtkWidget *text;

    gtk_init (&argc, &argv);
   
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "Paned Windows");
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
			GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    gtk_widget_set_usize (GTK_WIDGET(window), 450, 400);

    /* create a vpaned widget and add it to our toplevel window */
   
    vpaned = gtk_vpaned_new ();
    gtk_container_add (GTK_CONTAINER(window), vpaned);
    gtk_paned_set_handle_size (GTK_PANED(vpaned),
                               10);
    gtk_paned_set_gutter_size (GTK_PANED(vpaned),
                               15);                       
    gtk_widget_show (vpaned);
   
    /* Now create the contents of the two halves of the window */
   
    list = create_list ();
    gtk_paned_add1 (GTK_PANED(vpaned), list);
    gtk_widget_show (list);
   
    text = create_text ();
    gtk_paned_add2 (GTK_PANED(vpaned), text);
    gtk_widget_show (text);
    gtk_widget_show (window);
    gtk_main ();
    return 0;
}
/* example-end */
