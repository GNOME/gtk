/* Rotated Text
 *
 * This demo shows how to use GDK and Pango to draw rotated and transformed
 * text. The use of GdkPangoRenderer in this example is a somewhat advanced
 * technique; most applications can simply use gdk_draw_layout(). We use
 * it here mostly because that allows us to work in user coordinates - that is,
 * coordinates prior to the application of the transformation matrix, rather
 * than device coordinates.
 *
 * As of GTK+-2.6, the ability to draw transformed and anti-aliased graphics
 * as shown in this example is only present for text. With GTK+-2.8, a new
 * graphics system called "Cairo" will be introduced that provides these
 * capabilities and many more for all types of graphics.
 */
#include <math.h>
#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static gboolean
rotated_text_expose_event (GtkWidget      *widget,
			   GdkEventExpose *event,
			   gpointer	   data)
{
#define RADIUS 150
#define N_WORDS 10
#define FONT "Sans Bold 27"
  
  PangoRenderer *renderer;
  PangoMatrix matrix = PANGO_MATRIX_INIT;
  PangoContext *context;
  PangoLayout *layout;
  PangoFontDescription *desc;

  int width = widget->allocation.width;
  int height = widget->allocation.height;
  double device_radius;
  int i;

  /* Get the default renderer for the screen, and set it up for drawing  */
  renderer = gdk_pango_renderer_get_default (gtk_widget_get_screen (widget));
  gdk_pango_renderer_set_drawable (GDK_PANGO_RENDERER (renderer), widget->window);
  gdk_pango_renderer_set_gc (GDK_PANGO_RENDERER (renderer), widget->style->black_gc);

  /* Set up a transformation matrix so that the user space coordinates for
   * the centered square where we draw are [-RADIUS, RADIUS], [-RADIUS, RADIUS]
   * We first center, then change the scale */
  device_radius = MIN (width, height) / 2.;
  pango_matrix_translate (&matrix,
			  device_radius + (width - 2 * device_radius) / 2,
			  device_radius + (height - 2 * device_radius) / 2);
  pango_matrix_scale (&matrix, device_radius / RADIUS, device_radius / RADIUS);

  /* Create a PangoLayout, set the font and text */
  context = gtk_widget_create_pango_context (widget);
  layout = pango_layout_new (context);
  pango_layout_set_text (layout, "Text", -1);
  desc = pango_font_description_from_string (FONT);
  pango_layout_set_font_description (layout, desc);
  pango_font_description_free (desc);

  /* Draw the layout N_WORDS times in a circle */
  for (i = 0; i < N_WORDS; i++)
    {
      GdkColor color;
      PangoMatrix rotated_matrix = matrix;
      int width, height;
      double angle = (360. * i) / N_WORDS;

      /* Gradient from red at angle == 60 to blue at angle == 300 */
      color.red   = 65535 * (1 + cos ((angle - 60) * G_PI / 180.)) / 2;
      color.green = 0;
      color.blue  = 65535  - color.red;
    
      gdk_pango_renderer_set_override_color (GDK_PANGO_RENDERER (renderer),
					     PANGO_RENDER_PART_FOREGROUND, &color);
                                             
      pango_matrix_rotate (&rotated_matrix, angle);

      pango_context_set_matrix (context, &rotated_matrix);
    
      /* Inform Pango to re-layout the text with the new transformation matrix */
      pango_layout_context_changed (layout);
    
      pango_layout_get_size (layout, &width, &height);
      pango_renderer_draw_layout (renderer, layout,
				  - width / 2, - RADIUS * PANGO_SCALE);
    }

  /* Clean up default renderer, since it is shared */
  gdk_pango_renderer_set_override_color (GDK_PANGO_RENDERER (renderer),
					 PANGO_RENDER_PART_FOREGROUND, NULL);
  gdk_pango_renderer_set_drawable (GDK_PANGO_RENDERER (renderer), NULL);
  gdk_pango_renderer_set_gc (GDK_PANGO_RENDERER (renderer), NULL);

  /* free the objects we created */
  g_object_unref (layout);
  g_object_unref (context);
  
  return FALSE;
}

GtkWidget *
do_rotated_text (GtkWidget *do_widget)
{
  GtkWidget *drawing_area;
  
  if (!window)
    {
      const GdkColor white = { 0, 0xffff, 0xffff, 0xffff };
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Rotated Text");

      g_signal_connect (window, "destroy", G_CALLBACK (gtk_widget_destroyed), &window);

      drawing_area = gtk_drawing_area_new ();
      gtk_container_add (GTK_CONTAINER (window), drawing_area);

      /* This overrides the background color from the theme */
      gtk_widget_modify_bg (drawing_area, GTK_STATE_NORMAL, &white);

      g_signal_connect (drawing_area, "expose-event",
			G_CALLBACK (rotated_text_expose_event), NULL);

      gtk_window_set_default_size (GTK_WINDOW (window), 2 * RADIUS, 2 * RADIUS);
    }

  if (!GTK_WIDGET_VISIBLE (window))
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
