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
static void
draw_function (GtkDrawingArea *da,
               cairo_t        *cr,
               int             width,
               int             height,
               gpointer        data)
{
  gdk_cairo_set_source_rgba (cr, &color);
  cairo_paint (cr);
}

static void
response_cb (GtkDialog *dialog,
             gint       response_id,
             gpointer   user_data)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog), &color);
      gtk_widget_queue_draw (da);
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

  g_signal_connect (dialog, "response",
                    G_CALLBACK (response_cb), NULL);

  gtk_widget_show (dialog);
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

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      g_object_set (vbox, "margin", 12, NULL);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      /*
       * Create the color swatch area
       */

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (vbox), frame);

      da = gtk_drawing_area_new ();
      gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (da), 200);
      gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (da), 200);
      gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), draw_function, NULL, NULL);

      gtk_container_add (GTK_CONTAINER (frame), da);

      button = gtk_button_new_with_mnemonic ("_Change the above color");
      gtk_widget_set_halign (button, GTK_ALIGN_END);
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);

      gtk_box_pack_start (GTK_BOX (vbox), button);

      g_signal_connect (button, "clicked",
                        G_CALLBACK (change_color_callback), NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
