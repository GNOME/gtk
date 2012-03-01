/* Color Selector
 *
 * GtkColorSelection lets the user choose a color. GtkColorSelectionDialog is
 * a prebuilt dialog containing a GtkColorSelection.
 *
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;
static GtkWidget *da;
static GdkColor color;
static GtkWidget *frame;

/* Expose callback for the drawing area
 */
static gboolean
expose_event_callback (GtkWidget      *widget, 
                       GdkEventExpose *event, 
                       gpointer        data)
{
  GdkWindow *window = gtk_widget_get_window (widget);
  
  if (window)
    {
      GtkStyle *style;
      cairo_t *cr;

      style = gtk_widget_get_style (widget);

      cr = gdk_cairo_create (window);

      gdk_cairo_set_source_color (cr, &style->bg[GTK_STATE_NORMAL]);
      gdk_cairo_rectangle (cr, &event->area);
      cairo_fill (cr);

      cairo_destroy (cr);
    }

  return TRUE;
}

static void
change_color_callback (GtkWidget *button,
		       gpointer	  data)
{
  GtkWidget *dialog;
  GtkColorSelection *colorsel;
  gint response;
  
  dialog = gtk_color_selection_dialog_new ("Changing color");

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
  
  colorsel = 
    GTK_COLOR_SELECTION (gtk_color_selection_dialog_get_color_selection (GTK_COLOR_SELECTION_DIALOG (dialog)));

  gtk_color_selection_set_previous_color (colorsel, &color);
  gtk_color_selection_set_current_color (colorsel, &color);
  gtk_color_selection_set_has_palette (colorsel, TRUE);
  
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    {
      gtk_color_selection_get_current_color (colorsel,
					     &color);
      
      gtk_widget_modify_bg (da, GTK_STATE_NORMAL, &color);
    }
  
  gtk_widget_destroy (dialog);
}

GtkWidget *
do_colorsel (GtkWidget *do_widget)
{
  GtkWidget *vbox;
  GtkWidget *button;
  GtkWidget *alignment;
  
  if (!window)
    {
      color.red = 0;
      color.blue = 65535;
      color.green = 0;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Color Selection");

      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_container_set_border_width (GTK_CONTAINER (window), 8);

      vbox = gtk_vbox_new (FALSE, 8);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      /*
       * Create the color swatch area
       */
      
      
      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

      da = gtk_drawing_area_new ();

      g_signal_connect (da, "expose_event",
			G_CALLBACK (expose_event_callback), NULL);

      /* set a minimum size */
      gtk_widget_set_size_request (da, 200, 200);
      /* set the color */
      gtk_widget_modify_bg (da, GTK_STATE_NORMAL, &color);
      
      gtk_container_add (GTK_CONTAINER (frame), da);

      alignment = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
      
      button = gtk_button_new_with_mnemonic ("_Change the above color");
      gtk_container_add (GTK_CONTAINER (alignment), button);
      
      gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
      
      g_signal_connect (button, "clicked",
			G_CALLBACK (change_color_callback), NULL);
    }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
