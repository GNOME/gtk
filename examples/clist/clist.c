
#include "config.h"
#include <gtk/gtk.h>

/* User clicked the "Add List" button. */
void button_add_clicked( gpointer data )
{
    int indx;
 
    /* Something silly to add to the list. 4 rows of 2 columns each */
    gchar *drink[4][2] = { { "Milk",    "3 Oz" },
                           { "Water",   "6 l" },
                           { "Carrots", "2" },
                           { "Snakes",  "55" } };

    /* Here we do the actual adding of the text. It's done once for
     * each row.
     */
    for (indx = 0; indx < 4; indx++)
	gtk_clist_append ((GtkCList *)data, drink[indx]);

    return;
}

/* User clicked the "Clear List" button. */
void button_clear_clicked( gpointer data )
{
    /* Clear the list using gtk_clist_clear. This is much faster than
     * calling gtk_clist_remove once for each row.
     */
    gtk_clist_clear ((GtkCList *)data);

    return;
}

/* The user clicked the "Hide/Show titles" button. */
void button_hide_show_clicked( gpointer data )
{
    /* Just a flag to remember the status. 0 = currently visible */
    static short int flag = 0;

    if (flag == 0)
    {
        /* Hide the titles and set the flag to 1 */
	gtk_clist_column_titles_hide ((GtkCList *)data);
	flag++;
    }
    else
    {
        /* Show the titles and reset flag to 0 */
	gtk_clist_column_titles_show ((GtkCList *)data);
	flag--;
    }

    return;
}

/* If we come here, then the user has selected a row in the list. */
void selection_made( GtkWidget      *clist,
                     gint            row,
                     gint            column,
		     GdkEventButton *event,
                     gpointer        data )
{
    gchar *text;

    /* Get the text that is stored in the selected row and column
     * which was clicked in. We will receive it as a pointer in the
     * argument text.
     */
    gtk_clist_get_text (GTK_CLIST (clist), row, column, &text);

    /* Just prints some information about the selected row */
    g_print ("You selected row %d. More specifically you clicked in "
             "column %d, and the text in this cell is %s\n\n",
	     row, column, text);

    return;
}

int main( int    argc,
          gchar *argv[] )
{                                  
    GtkWidget *window;
    GtkWidget *vbox, *hbox;
    GtkWidget *scrolled_window, *clist;
    GtkWidget *button_add, *button_clear, *button_hide_show;    
    gchar *titles[2] = { "Ingredients", "Amount" };

    gtk_init(&argc, &argv);
    
    window=gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_size_request (GTK_WIDGET (window), 300, 150);

    gtk_window_set_title (GTK_WINDOW (window), "GtkCList Example");
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (gtk_main_quit),
                      NULL);
    
    vbox=gtk_vbox_new (FALSE, 5);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
    gtk_container_add (GTK_CONTAINER (window), vbox);
    gtk_widget_show (vbox);
    
    /* Create a scrolled window to pack the CList widget into */
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

    gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
    gtk_widget_show (scrolled_window);

    /* Create the CList. For this example we use 2 columns */
    clist = gtk_clist_new_with_titles (2, titles);

    /* When a selection is made, we want to know about it. The callback
     * used is selection_made, and its code can be found further down */
    g_signal_connect (G_OBJECT (clist), "select_row",
                      G_CALLBACK (selection_made),
                      NULL);

    /* It isn't necessary to shadow the border, but it looks nice :) */
    gtk_clist_set_shadow_type (GTK_CLIST (clist), GTK_SHADOW_OUT);

    /* What however is important, is that we set the column widths as
     * they will never be right otherwise. Note that the columns are
     * numbered from 0 and up (to 1 in this case).
     */
    gtk_clist_set_column_width (GTK_CLIST (clist), 0, 150);

    /* Add the CList widget to the vertical box and show it. */
    gtk_container_add (GTK_CONTAINER (scrolled_window), clist);
    gtk_widget_show (clist);

    /* Create the buttons and add them to the window. See the button
     * tutorial for more examples and comments on this.
     */
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show (hbox);

    button_add = gtk_button_new_with_label ("Add List");
    button_clear = gtk_button_new_with_label ("Clear List");
    button_hide_show = gtk_button_new_with_label ("Hide/Show titles");

    gtk_box_pack_start (GTK_BOX (hbox), button_add, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), button_clear, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), button_hide_show, TRUE, TRUE, 0);

    /* Connect our callbacks to the three buttons */
    g_signal_connect_swapped (G_OBJECT (button_add), "clicked",
                              G_CALLBACK (button_add_clicked),
			      clist);
    g_signal_connect_swapped (G_OBJECT (button_clear), "clicked",
                              G_CALLBACK (button_clear_clicked),
                              clist);
    g_signal_connect_swapped (G_OBJECT (button_hide_show), "clicked",
                              G_CALLBACK (button_hide_show_clicked),
                              clist);

    gtk_widget_show (button_add);
    gtk_widget_show (button_clear);
    gtk_widget_show (button_hide_show);

    /* The interface is completely set up so we show the window and
     * enter the gtk_main loop.
     */
    gtk_widget_show (window);

    gtk_main();
    
    return 0;
}
