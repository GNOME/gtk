/* This file extracted from the GTK tutorial. */

/* helloworld2.c */

#include <gtk/gtk.h>
   
/* Our new improved callback.  The data passed to this function is printed
 * to stdout. */
void callback (GtkWidget *widget, gpointer data)
{  
    g_print ("Hello again - %s was pressed\n", (char *) data);
}
     
/* another callback */
void delete_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{    
    gtk_main_quit ();
}

int main (int argc, char *argv[])
{
    /* GtkWidget is the storage type for widgets */
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *box1;

    /* this is called in all GTK applications.  arguments are parsed from
     * the command line and are returned to the application. */
    gtk_init (&argc, &argv);

    /* create a new window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    /* this is a new call, this just sets the title of our
     * new window to "Hello Buttons!" */
    gtk_window_set_title (GTK_WINDOW (window), "Hello Buttons!");

    /* Here we just set a handler for delete_event that immediately
     * exits GTK. */
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                        GTK_SIGNAL_FUNC (delete_event), NULL);

    
    /* sets the border width of the window. */
    gtk_container_border_width (GTK_CONTAINER (window), 10);

    /* we create a box to pack widgets into.  this is described in detail
     * in the "packing" section below.  The box is not really visible, it
     * is just used as a tool to arrange widgets. */
    box1 = gtk_hbox_new(FALSE, 0);

    /* put the box into the main window. */
    gtk_container_add (GTK_CONTAINER (window), box1);

    /* creates a new button with the label "Button 1". */
    button = gtk_button_new_with_label ("Button 1");
    
    /* Now when the button is clicked, we call the "callback" function
     * with a pointer to "button 1" as it's argument */
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        GTK_SIGNAL_FUNC (callback), (gpointer) "button 1");
                        
    /* instead of gtk_container_add, we pack this button into the invisible
     * box, which has been packed into the window. */
    gtk_box_pack_start(GTK_BOX(box1), button, TRUE, TRUE, 0);
    
    /* always remember this step, this tells GTK that our preparation for
     * this button is complete, and it can be displayed now. */
    gtk_widget_show(button);
     
    /* do these same steps again to create a second button */
    button = gtk_button_new_with_label ("Button 2");
    
    /* call the same callback function with a different argument,
     * passing a pointer to "button 2" instead. */
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        GTK_SIGNAL_FUNC (callback), (gpointer) "button 2");
    
    gtk_box_pack_start(GTK_BOX(box1), button, TRUE, TRUE, 0);
     
    /* The order in which we show the buttons is not really important, but I
     * recommend showing the window last, so it all pops up at once. */
    gtk_widget_show(button);

    gtk_widget_show(box1);

    gtk_widget_show (window);

    /* rest in gtk_main and wait for the fun to begin! */
    gtk_main ();
     
    return 0;
}

