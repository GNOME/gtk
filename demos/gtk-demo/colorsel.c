/* Color Chooser
 *
 * A GtkColorChooser lets the user choose a color. There are several
 * implementations of the GtkColorChooser interface in GTK+. The
 * GtkColorChooserDialog is a prebuilt dialog containing a
 * GtkColorChooserWidget.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;
static GtkWidget *da;
static GdkRGBA color;
static GtkWidget *frame;

/* draw callback for the drawing area
 */
static gboolean
draw_callback (GtkWidget *widget,
               cairo_t   *cr,
               gpointer   data)
{
  GtkStyleContext *context;
  GdkRGBA rgba;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &rgba);
  gdk_cairo_set_source_rgba (cr, &rgba);
  cairo_paint (cr);

  return TRUE;
}

static void
response_cb (GtkDialog *dialog,
             gint       response_id,
             gpointer   user_data)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog), &color);
      gtk_widget_override_background_color (da, 0, &color);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
change_color_callback (GtkWidget *button,
                       gpointer   data)
{
  GtkWidget *dialog;

  dialog = gtk_color_chooser_dialog_new ("Changing color", GTK_WINDOW (window));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dialog), &color);

  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (response_cb),
                    NULL);

  gtk_widget_show_all (dialog);
}

GtkWidget *
do_colorsel (GtkWidget *do_widget)
{
  GtkWidget *vbox;
  GtkWidget *button;

  if (!window)
    {
      color.red = 0;
      color.blue = 1;
      color.green = 0;
      color.alpha = 1;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Color Chooser");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_container_set_border_width (GTK_CONTAINER (window), 8);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      /*
       * Create the color swatch area
       */


      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

      da = gtk_drawing_area_new ();

      g_signal_connect (da, "draw", G_CALLBACK (draw_callback), NULL);

      /* set a minimum size */
      gtk_widget_set_size_request (da, 200, 200);
      /* set the color */
      gtk_widget_override_background_color (da, 0, &color);

      gtk_container_add (GTK_CONTAINER (frame), da);

      button = gtk_button_new_with_mnemonic ("_Change the above color");
      gtk_widget_set_halign (button, GTK_ALIGN_END);
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);

      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

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
