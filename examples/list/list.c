/* This file extracted from the GTK tutorial. */

/* list.c */

/* include the gtk+ header files
 * include stdio.h, we need that for the printf() function
 */
#include        <gtk/gtk.h>
#include        <stdio.h>

/* this is our data identification string to store
 * data in list items
 */
const   gchar   *list_item_data_key="list_item_data";


/* prototypes for signal handler that we are going to connect
 * to the GtkList widget
 */
static  void    sigh_print_selection    (GtkWidget      *gtklist,
                                         gpointer       func_data);
static  void    sigh_button_event       (GtkWidget      *gtklist,
                                         GdkEventButton *event,
                                         GtkWidget      *frame);


/* main function to set up the user interface */

gint main (int argc, gchar *argv[])
{                                  
    GtkWidget       *separator;
    GtkWidget       *window;
    GtkWidget       *vbox;
    GtkWidget       *scrolled_window;
    GtkWidget       *frame;
    GtkWidget       *gtklist;
    GtkWidget       *button;
    GtkWidget       *list_item;
    GList           *dlist;
    guint           i;
    gchar           buffer[64];
    
    
    /* initialize gtk+ (and subsequently gdk) */

    gtk_init(&argc, &argv);
    
    
    /* create a window to put all the widgets in
     * connect gtk_main_quit() to the "destroy" event of
     * the window to handle window manager close-window-events
     */
    window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "GtkList Example");
    gtk_signal_connect(GTK_OBJECT(window),
		       "destroy",
		       GTK_SIGNAL_FUNC(gtk_main_quit),
		       NULL);
    
    
    /* inside the window we need a box to arrange the widgets
     * vertically */
    vbox=gtk_vbox_new(FALSE, 5);
    gtk_container_border_width(GTK_CONTAINER(vbox), 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show(vbox);
    
    /* this is the scolled window to put the GtkList widget inside */
    scrolled_window=gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_usize(scrolled_window, 250, 150);
    gtk_container_add(GTK_CONTAINER(vbox), scrolled_window);
    gtk_widget_show(scrolled_window);
    
    /* create the GtkList widget
     * connect the sigh_print_selection() signal handler
     * function to the "selection_changed" signal of the GtkList
     * to print out the selected items each time the selection
     * has changed */
    gtklist=gtk_list_new();
    gtk_container_add(GTK_CONTAINER(scrolled_window), gtklist);
    gtk_widget_show(gtklist);
    gtk_signal_connect(GTK_OBJECT(gtklist),
		       "selection_changed",
		       GTK_SIGNAL_FUNC(sigh_print_selection),
		       NULL);
    
    /* we create a "Prison" to put a list item in ;)
     */
    frame=gtk_frame_new("Prison");
    gtk_widget_set_usize(frame, 200, 50);
    gtk_container_border_width(GTK_CONTAINER(frame), 5);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
    gtk_container_add(GTK_CONTAINER(vbox), frame);
    gtk_widget_show(frame);
    
    /* connect the sigh_button_event() signal handler to the GtkList
     * wich will handle the "arresting" of list items
     */
    gtk_signal_connect(GTK_OBJECT(gtklist),
		       "button_release_event",
		       GTK_SIGNAL_FUNC(sigh_button_event),
		       frame);
    
    /* create a separator
     */
    separator=gtk_hseparator_new();
    gtk_container_add(GTK_CONTAINER(vbox), separator);
    gtk_widget_show(separator);
    
    /* finaly create a button and connect it´s "clicked" signal
     * to the destroyment of the window
     */
    button=gtk_button_new_with_label("Close");
    gtk_container_add(GTK_CONTAINER(vbox), button);
    gtk_widget_show(button);
    gtk_signal_connect_object(GTK_OBJECT(button),
			      "clicked",
			      GTK_SIGNAL_FUNC(gtk_widget_destroy),
			      GTK_OBJECT(window));
    
    
    /* now we create 5 list items, each having it´s own
     * label and add them to the GtkList using gtk_container_add()
     * also we query the text string from the label and
     * associate it with the list_item_data_key for each list item
     */
    for (i=0; i<5; i++) {
	GtkWidget       *label;
	gchar           *string;
	
	sprintf(buffer, "ListItemContainer with Label #%d", i);
	label=gtk_label_new(buffer);
	list_item=gtk_list_item_new();
	gtk_container_add(GTK_CONTAINER(list_item), label);
	gtk_widget_show(label);
	gtk_container_add(GTK_CONTAINER(gtklist), list_item);
	gtk_widget_show(list_item);
	gtk_label_get(GTK_LABEL(label), &string);
	gtk_object_set_data(GTK_OBJECT(list_item),
			    list_item_data_key,
			    string);
    }
    /* here, we are creating another 5 labels, this time
     * we use gtk_list_item_new_with_label() for the creation
     * we can´t query the text string from the label because
     * we don´t have the labels pointer and therefore
     * we just associate the list_item_data_key of each
     * list item with the same text string
     * for adding of the list items we put them all into a doubly
     * linked list (GList), and then add them by a single call to
     * gtk_list_append_items()
     * because we use g_list_prepend() to put the items into the
     * doubly linked list, their order will be descending (instead
     * of ascending when using g_list_append())
     */
    dlist=NULL;
    for (; i<10; i++) {
	sprintf(buffer, "List Item with Label %d", i);
	list_item=gtk_list_item_new_with_label(buffer);
	dlist=g_list_prepend(dlist, list_item);
	gtk_widget_show(list_item);
	gtk_object_set_data(GTK_OBJECT(list_item),
			    list_item_data_key,
			    "ListItem with integrated Label");
    }
    gtk_list_append_items(GTK_LIST(gtklist), dlist);
    
    /* finaly we want to see the window, don´t we? ;)
     */
    gtk_widget_show(window);
    
    /* fire up the main event loop of gtk
     */
    gtk_main();
    
    /* we get here after gtk_main_quit() has been called which
     * happens if the main window gets destroyed
     */
    return 0;
}

/* this is the signal handler that got connected to button
 * press/release events of the GtkList
 */
void
sigh_button_event       (GtkWidget      *gtklist,
                         GdkEventButton *event,
                         GtkWidget      *frame)
{
    /* we only do something if the third (rightmost mouse button
     * was released
     */
    if (event->type==GDK_BUTTON_RELEASE &&
	event->button==3) {
	GList           *dlist, *free_list;
	GtkWidget       *new_prisoner;
	
	/* fetch the currently selected list item which
	 * will be our next prisoner ;)
	 */
	dlist=GTK_LIST(gtklist)->selection;
	if (dlist)
		new_prisoner=GTK_WIDGET(dlist->data);
	else
		new_prisoner=NULL;
	
	/* look for already prisoned list items, we
	 * will put them back into the list
	 * remember to free the doubly linked list that
	 * gtk_container_children() returns
	 */
	dlist=gtk_container_children(GTK_CONTAINER(frame));
	free_list=dlist;
	while (dlist) {
	    GtkWidget       *list_item;
	    
	    list_item=dlist->data;
	    
	    gtk_widget_reparent(list_item, gtklist);
	    
	    dlist=dlist->next;
	}
	g_list_free(free_list);
	
	/* if we have a new prisoner, remove him from the
	 * GtkList and put him into the frame "Prison"
	 * we need to unselect the item before
	 */
	if (new_prisoner) {
	    GList   static_dlist;
	    
	    static_dlist.data=new_prisoner;
	    static_dlist.next=NULL;
	    static_dlist.prev=NULL;
	    
	    gtk_list_unselect_child(GTK_LIST(gtklist),
				    new_prisoner);
	    gtk_widget_reparent(new_prisoner, frame);
	}
    }
}

/* this is the signal handler that gets called if GtkList
 * emits the "selection_changed" signal
 */
void
sigh_print_selection    (GtkWidget      *gtklist,
                         gpointer       func_data)
{
    GList   *dlist;
    
    /* fetch the doubly linked list of selected items
     * of the GtkList, remember to treat this as read-only!
     */
    dlist=GTK_LIST(gtklist)->selection;
    
    /* if there are no selected items there is nothing more
     * to do than just telling the user so
     */
    if (!dlist) {
	g_print("Selection cleared\n");
	return;
    }
    /* ok, we got a selection and so we print it
     */
    g_print("The selection is a ");
    
    /* get the list item from the doubly linked list
     * and then query the data associated with list_item_data_key
     * we then just print it
     */
    while (dlist) {
	GtkObject       *list_item;
	gchar           *item_data_string;
	
	list_item=GTK_OBJECT(dlist->data);
	item_data_string=gtk_object_get_data(list_item,
					     list_item_data_key);
	g_print("%s ", item_data_string);
	
	dlist=dlist->next;
    }
    g_print("\n");
}
