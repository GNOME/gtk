/* This file extracted from the GTK tutorial. */

/* helloworld.c */
               
#include <gtk/gtk.h>

/* this is a callback function. the data arguments are ignored in this example.
 * More on callbacks below. */  
void hello (GtkWidget *widget, gpointer data)
{
    g_print ("Hello World\n");
}
     

gint delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    g_print ("delete event occured\n");
    /* if you return FALSE in the "delete_event" signal handler,
     * GTK will emit the "destroy" signal.  Returning TRUE means
     * you don't want the window to be destroyed.
     * This is useful for popping up 'are you sure you want to quit ?'
     * type dialogs. */

    /* Change TRUE to FALSE and the main window will be destroyed with
     * a "delete_event". */

    return (TRUE);
}

/* another callback */
void destroy (GtkWidget *widget, gpointer data)
{
    gtk_main_quit ();
}

int main (int argc, char *argv[])
{
    /* GtkWidget is the storage type for widgets */
    GtkWidget *window;
    GtkWidget *button;

    /* this is called in all GTK applications.  arguments are parsed from
     * the command line and are returned to the application. */
    gtk_init (&argc, &argv);
    
    /* create a new window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    /* when the window is given the "delete_event" signal (this is given
     * by the window manager (usually the 'close' option, or on the
     * titlebar), we ask it to call the delete_event () function
     * as defined above.  The data passed to the callback
     * function is NULL and is ignored in the callback. */
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                        GTK_SIGNAL_FUNC (delete_event), NULL);

    /* here we connect the "destroy" event to a signal handler.
     * This event occurs when we call gtk_widget_destroy() on the window,
     * or if we return 'FALSE' in the "delete_event" callback. */
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
                        GTK_SIGNAL_FUNC (destroy), NULL);

    /* sets the border width of the window. */
    gtk_container_border_width (GTK_CONTAINER (window), 10);

    /* creates a new button with the label "Hello World". */
    button = gtk_button_new_with_label ("Hello World");

    /* When the button receives the "clicked" signal, it will call the
     * function hello() passing it NULL as it's argument.  The hello() 
     * function is defined above. */
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        GTK_SIGNAL_FUNC (hello), NULL);
    
    /* This will cause the window to be destroyed by calling
     * gtk_widget_destroy(window) when "clicked".  Again, the destroy
     * signal could come from here, or the window manager. */
    gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                               GTK_SIGNAL_FUNC (gtk_widget_destroy),
                               GTK_OBJECT (window));
                        
    /* this packs the button into the window (a gtk container). */
    gtk_container_add (GTK_CONTAINER (window), button);
    
    /* the final step is to display this newly created widget... */
    gtk_widget_show (button);

    /* and the window */
    gtk_widget_show (window);
     
    /* all GTK applications must have a gtk_main().     Control ends here
     * and waits for an event to occur (like a key press or mouse event). */
    gtk_main ();
                        
    return 0;
}

