
#include <gtk/gtk.h>

/* I'm going to be lazy and use some global variables to
 * store the position of the widget within the fixed
 * container */
gint x = 50;
gint y = 50;

/* This callback function moves the button to a new position
 * in the Fixed container. */
static void move_button( GtkWidget *widget,
                         GtkWidget *fixed )
{
  x = (x + 30) % 300;
  y = (y + 50) % 300;
  gtk_fixed_move (GTK_FIXED (fixed), widget, x, y); 
}

int main( int   argc,
          char *argv[] )
{
  /* GtkWidget is the storage type for widgets */
  GtkWidget *window;
  GtkWidget *fixed;
  GtkWidget *button;
  gint i;

  /* Initialise GTK */
  gtk_init (&argc, &argv);
    
  /* Create a new window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Fixed Container");

  /* Here we connect the "destroy" event to a signal handler */ 
  g_signal_connect (G_OBJECT (window), "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);
 
  /* Sets the border width of the window. */
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);

  /* Create a Fixed Container */
  fixed = gtk_fixed_new ();
  gtk_container_add (GTK_CONTAINER (window), fixed);
  gtk_widget_show (fixed);
  
  for (i = 1 ; i <= 3 ; i++) {
    /* Creates a new button with the label "Press me" */
    button = gtk_button_new_with_label ("Press me");
  
    /* When the button receives the "clicked" signal, it will call the
     * function move_button() passing it the Fixed Container as its
     * argument. */
    g_signal_connect (G_OBJECT (button), "clicked",
		      G_CALLBACK (move_button), (gpointer) fixed);
  
    /* This packs the button into the fixed containers window. */
    gtk_fixed_put (GTK_FIXED (fixed), button, i*50, i*50);
  
    /* The final step is to display this newly created widget. */
    gtk_widget_show (button);
  }

  /* Display the window */
  gtk_widget_show (window);
    
  /* Enter the event loop */
  gtk_main ();
    
  return 0;
}
