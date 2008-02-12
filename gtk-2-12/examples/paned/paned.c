
#include <stdio.h>
#include <gtk/gtk.h>
   
/* Create the list of "messages" */
static GtkWidget *create_list( void )
{

    GtkWidget *scrolled_window;
    GtkWidget *tree_view;
    GtkListStore *model;
    GtkTreeIter iter;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;

    int i;
   
    /* Create a new scrolled window, with scrollbars only if needed */
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				    GTK_POLICY_AUTOMATIC, 
				    GTK_POLICY_AUTOMATIC);
   
    model = gtk_list_store_new (1, G_TYPE_STRING);
    tree_view = gtk_tree_view_new ();
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), 
                                           tree_view);
    gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (model));
    gtk_widget_show (tree_view);
   
    /* Add some messages to the window */
    for (i = 0; i < 10; i++) {
        gchar *msg = g_strdup_printf ("Message #%d", i);
        gtk_list_store_append (GTK_LIST_STORE (model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), 
	                    &iter,
                            0, msg,
	                    -1);
	g_free (msg);
    }
   
    cell = gtk_cell_renderer_text_new ();

    column = gtk_tree_view_column_new_with_attributes ("Messages",
                                                       cell,
                                                       "text", 0,
                                                       NULL);
  
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
	  		         GTK_TREE_VIEW_COLUMN (column));

    return scrolled_window;
}
   
/* Add some text to our text widget - this is a callback that is invoked
when our window is realized. We could also force our window to be
realized with gtk_widget_realize, but it would have to be part of
a hierarchy first */

static void insert_text( GtkTextBuffer *buffer )
{
   GtkTextIter iter;
 
   gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);

   gtk_text_buffer_insert (buffer, &iter,   
    "From: pathfinder@nasa.gov\n"
    "To: mom@nasa.gov\n"
    "Subject: Made it!\n"
    "\n"
    "We just got in this morning. The weather has been\n"
    "great - clear but cold, and there are lots of fun sights.\n"
    "Sojourner says hi. See you soon.\n"
    " -Path\n", -1);
}
   
/* Create a scrolled text area that displays a "message" */
static GtkWidget *create_text( void )
{
   GtkWidget *scrolled_window;
   GtkWidget *view;
   GtkTextBuffer *buffer;

   view = gtk_text_view_new ();
   buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

   scrolled_window = gtk_scrolled_window_new (NULL, NULL);
   gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
		   	           GTK_POLICY_AUTOMATIC,
				   GTK_POLICY_AUTOMATIC);

   gtk_container_add (GTK_CONTAINER (scrolled_window), view);
   insert_text (buffer);

   gtk_widget_show_all (scrolled_window);

   return scrolled_window;
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
    g_signal_connect (G_OBJECT (window), "destroy",
	              G_CALLBACK (gtk_main_quit), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    gtk_widget_set_size_request (GTK_WIDGET (window), 450, 400);

    /* create a vpaned widget and add it to our toplevel window */
   
    vpaned = gtk_vpaned_new ();
    gtk_container_add (GTK_CONTAINER (window), vpaned);
    gtk_widget_show (vpaned);
   
    /* Now create the contents of the two halves of the window */
   
    list = create_list ();
    gtk_paned_add1 (GTK_PANED (vpaned), list);
    gtk_widget_show (list);
   
    text = create_text ();
    gtk_paned_add2 (GTK_PANED (vpaned), text);
    gtk_widget_show (text);
    gtk_widget_show (window);

    gtk_main ();

    return 0;
}
