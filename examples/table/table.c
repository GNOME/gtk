/* This file extracted from the GTK tutorial. */

/* table.c */
#include <gtk/gtk.h>

/* our callback.
 * the data passed to this function is printed to stdout */
void callback (GtkWidget *widget, gpointer *data)
{
    g_print ("Hello again - %s was pressed\n", (char *) data);
}

/* this callback quits the program */
void delete_event (GtkWidget *widget, gpointer *data)
{
    gtk_main_quit ();
}

int main (int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *table;

    gtk_init (&argc, &argv);

    /* create a new window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    /* set the window title */
    gtk_window_set_title (GTK_WINDOW (window), "Table");

    /* set a handler for delete_event that immediately
     * exits GTK. */
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                        GTK_SIGNAL_FUNC (delete_event), NULL);

    /* sets the border width of the window. */
    gtk_container_border_width (GTK_CONTAINER (window), 20);

    /* create a 2x2 table */
    table = gtk_table_new (2, 2, TRUE);

    /* put the table in the main window */
    gtk_container_add (GTK_CONTAINER (window), table);

    /* create first button */
    button = gtk_button_new_with_label ("button 1");

    /* when the button is clicked, we call the "callback" function
     * with a pointer to "button 1" as it's argument */
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
              GTK_SIGNAL_FUNC (callback), (gpointer) "button 1");


    /* insert button 1 into the upper left quadrant of the table */
    gtk_table_attach_defaults (GTK_TABLE(table), button, 0, 1, 0, 1);

    gtk_widget_show (button);

    /* create second button */

    button = gtk_button_new_with_label ("button 2");

    /* when the button is clicked, we call the "callback" function
     * with a pointer to "button 2" as it's argument */
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
              GTK_SIGNAL_FUNC (callback), (gpointer) "button 2");
    /* insert button 2 into the upper right quadrant of the table */
    gtk_table_attach_defaults (GTK_TABLE(table), button, 1, 2, 0, 1);

    gtk_widget_show (button);

    /* create "Quit" button */
    button = gtk_button_new_with_label ("Quit");

    /* when the button is clicked, we call the "delete_event" function
     * and the program exits */
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        GTK_SIGNAL_FUNC (delete_event), NULL);

    /* insert the quit button into the both 
     * lower quadrants of the table */
    gtk_table_attach_defaults (GTK_TABLE(table), button, 0, 2, 1, 2);

    gtk_widget_show (button);

    gtk_widget_show (table);
    gtk_widget_show (window);

    gtk_main ();

    return 0;
}
