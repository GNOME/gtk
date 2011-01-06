#include <gtk/gtk.h>

/* This is a callback function. The data arguments are ignored
 * in this example. More on callbacks below.
 */
static void
print_hello (GtkWidget *widget,
             gpointer   data)
{
  g_print ("Hello World\n");
}

static gboolean
on_delete_event (GtkWidget *widget,
                 GdkEvent  *event,
                 gpointer   data)
{
  /* If you return FALSE in the "delete_event" signal handler,
   * GTK will emit the "destroy" signal. Returning TRUE means
   * you don't want the window to be destroyed.
   *
   * This is useful for popping up 'are you sure you want to quit?'
   * type dialogs.
   */

  g_print ("delete event occurred\n");

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  /* GtkWidget is the storage type for widgets */
  GtkWidget *window;
  GtkWidget *button;

  /* This is called in all GTK applications. Arguments are parsed
   * from the command line and are returned to the application.
   */
  gtk_init (&argc, &argv);

  /* create a new window, and set its title */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Hello");

  /* When the window emits the "delete-event" signal (which is emitted
   * by GTK+ in response to an event coming from the window manager,
   * usually as a result of clicking the "close" window control), we
   * ask it to call the on_delete_event() function as defined above.
   *
   * The data passed to the callback function is NULL and is ignored
   * in the callback function.
   */
  g_signal_connect (window, "delete-event", G_CALLBACK (on_delete_event), NULL);

  /* Here we connect the "destroy" event to the gtk_main_quit() function.
   *
   * This signal is emitted when we call gtk_widget_destroy() on the window,
   * or if we return FALSE in the "delete_event" callback.
   */
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  /* Sets the border width of the window. */
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);

  /* Creates a new button with the label "Hello World". */
  button = gtk_button_new_with_label ("Hello World");

  /* When the button receives the "clicked" signal, it will call the
   * function print_hello() passing it NULL as its argument.
   *
   * The print_hello() function is defined above.
   */
  g_signal_connect (button, "clicked", G_CALLBACK (print_hello), NULL);

  /* The g_signal_connect_swapped() function will connect the "clicked" signal
   * of the button to the gtk_widget_destroy() function; instead of calling it
   * using the button as its argument, it will swap it with the user data
   * argument. This will cause the window to be destroyed by calling
   * gtk_widget_destroy() on the window.
   */
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), window);

  /* This packs the button into the window. A GtkWindow inherits from GtkBin,
   * which is a special container that can only have one child
   */
  gtk_container_add (GTK_CONTAINER (window), button);

  /* The final step is to display this newly created widget... */
  gtk_widget_show (button);

  /* ... and the window */
  gtk_widget_show (window);

  /* All GTK applications must have a gtk_main(). Control ends here
   * and waits for an event to occur (like a key press or a mouse event),
   * until gtk_main_quit() is called.
   */
  gtk_main ();

  return 0;
}
