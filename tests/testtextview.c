

#include "config.h"
#include <gtk/gtk.h>


static void
create_tags (GtkTextBuffer *buffer)
{

  gtk_text_buffer_create_tag (buffer, "italic",
                              "style", PANGO_STYLE_ITALIC, NULL);

  gtk_text_buffer_create_tag (buffer, "bold",
                              "weight", PANGO_WEIGHT_BOLD, NULL);

  gtk_text_buffer_create_tag (buffer, "x-large",
                              "scale", PANGO_SCALE_X_LARGE, NULL);

  gtk_text_buffer_create_tag (buffer, "semi_blue_foreground",
                              "foreground", "rgba(0,0,255,0.5)", NULL);

  gtk_text_buffer_create_tag (buffer, "semi_red_background",
                              "background", "rgba(255,0,0,0.5)", NULL);

  gtk_text_buffer_create_tag (buffer, "word_wrap",
                              "wrap_mode", GTK_WRAP_WORD, NULL);
}


static void
insert_text (GtkTextBuffer *buffer)
{
  GtkTextIter iter;
  GtkTextIter start, end;

  /* get start of buffer; each insertion will revalidate the
   * iterator to point to just after the inserted text.
   */
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);

  gtk_text_buffer_insert (buffer, &iter,
      "This test shows text view rendering some text with rgba colors.\n\n", -1);

  gtk_text_buffer_insert (buffer, &iter, "For example, you can have ", -1);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "italic translucent blue text", -1,
                                            "italic", 
					    "semi_blue_foreground",
					    "x-large",
					    NULL);

  gtk_text_buffer_insert (buffer, &iter, ", or ", -1);

  gtk_text_buffer_insert (buffer, &iter, ", ", -1);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "bold text with translucent red background", -1,
                                            "bold", 
					    "semi_red_background",
					    "x-large",
					    NULL);
  gtk_text_buffer_insert (buffer, &iter, ".", -1);

  /* Apply word_wrap tag to whole buffer */
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  gtk_text_buffer_apply_tag_by_name (buffer, "word_wrap", &start, &end);
}


static cairo_pattern_t *
get_pattern (void)
{
  static cairo_pattern_t *static_pattern = NULL;

  if (!static_pattern)
    {
      cairo_surface_t *surface = 
	cairo_image_surface_create_from_png ("gradient1.png");

      if (surface)
	{
	  static_pattern = cairo_pattern_create_for_surface (surface);
	  cairo_pattern_set_extend (static_pattern, CAIRO_EXTEND_REFLECT);
	}
      else 
	g_warning ("Failed to create surface for gradient1.png\n");
    }
  return static_pattern;
}

static void
draw_background (GtkWidget *widget, cairo_t *cr)
{
  GtkAllocation allocation;
  cairo_pattern_t *pat;
  
  gtk_widget_get_allocation (widget, &allocation);

  cairo_save (cr);

#if 0
  pat = cairo_pattern_create_linear (0.0, 0.0,  30.0, 30.0);
  cairo_pattern_add_color_stop_rgba (pat, 1, 0, 0, 0, 1);
  cairo_pattern_add_color_stop_rgba (pat, 0, 1, 1, 1, 1);
  cairo_pattern_set_extend (pat, CAIRO_EXTEND_REPEAT);
  cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
  cairo_set_source (cr, pat);
  cairo_fill (cr);
  cairo_pattern_destroy (pat);

#else

  if (get_pattern ())
    {
      cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
      cairo_set_source (cr, get_pattern ());
      cairo_fill (cr);
    }
#endif

  cairo_restore (cr);
}

int
main (int argc, char **argv)
{
  GtkWidget *window, *textview;
  GtkTextBuffer *buffer;

  gtk_init (&argc, &argv);

  window   = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  textview = gtk_text_view_new ();
  buffer   = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));

  create_tags (buffer);
  insert_text (buffer);
  
  gtk_widget_show (textview);
  gtk_container_add (GTK_CONTAINER (window), textview);

  g_signal_connect (textview, "draw",
		    G_CALLBACK (draw_background), NULL);


  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
