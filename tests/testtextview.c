

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
                              "foreground", "rgba(0,0,255,0.7)", NULL);

  gtk_text_buffer_create_tag (buffer, "semi_red_background",
                              "background", "rgba(255,0,0,0.5)", NULL);

  gtk_text_buffer_create_tag (buffer, "semi_orange_paragraph_background",
                              "paragraph-background", "rgba(255,165,0,0.5)", NULL);

  gtk_text_buffer_create_tag (buffer, "word_wrap",
                              "wrap_mode", GTK_WRAP_WORD, NULL);
}


static void
insert_text (GtkTextBuffer *buffer)
{
  GtkTextIter  iter;
  GtkTextIter  start, end;
  GtkTextMark *para_start;

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
  gtk_text_buffer_insert (buffer, &iter, ".\n\n", -1);

  /* Store the beginning of the other paragraph */
  para_start = gtk_text_buffer_create_mark (buffer, "para_start", &iter, TRUE);

  gtk_text_buffer_insert (buffer, &iter,
      "Paragraph background colors can also be set with rgba color values .\n", -1);

  gtk_text_buffer_insert (buffer, &iter, "For instance, you can have ", -1);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "bold translucent blue text", -1,
                                            "bold", 
					    "semi_blue_foreground",
					    "x-large",
					    NULL);

  gtk_text_buffer_insert (buffer, &iter, ", or ", -1);

  gtk_text_buffer_insert (buffer, &iter, ", ", -1);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "italic text with translucent red background", -1,
                                            "italic", 
					    "semi_red_background",
					    "x-large",
					    NULL);

  gtk_text_buffer_insert (buffer, &iter, " all rendered onto a translucent orange paragraph background.\n", -1);

  gtk_text_buffer_get_bounds (buffer, &start, &end);

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, para_start);
  gtk_text_buffer_apply_tag_by_name (buffer, "semi_orange_paragraph_background", &iter, &end);

  /* Apply word_wrap tag to whole buffer */
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  gtk_text_buffer_apply_tag_by_name (buffer, "word_wrap", &start, &end);
}


/* Size of checks and gray levels for alpha compositing checkerboard */
#define CHECK_SIZE  10
#define CHECK_DARK  (1.0 / 3.0)
#define CHECK_LIGHT (2.0 / 3.0)

static cairo_pattern_t *
get_checkered (void)
{
  /* need to respect pixman's stride being a multiple of 4 */
  static unsigned char data[8] = { 0xFF, 0x00, 0x00, 0x00,
                                   0x00, 0xFF, 0x00, 0x00 };
  static cairo_surface_t *checkered = NULL;
  cairo_pattern_t *pattern;

  if (checkered == NULL)
    {
      checkered = cairo_image_surface_create_for_data (data,
                                                       CAIRO_FORMAT_A8,
                                                       2, 2, 4);
    }

  pattern = cairo_pattern_create_for_surface (checkered);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);

  return pattern;
}

static void
draw_background (GtkWidget *widget, cairo_t *cr)
{
  cairo_pattern_t *pat;

  cairo_save (cr);

  cairo_set_source_rgb (cr, CHECK_DARK, CHECK_DARK, CHECK_DARK);
  cairo_paint (cr);

  cairo_set_source_rgb (cr, CHECK_LIGHT, CHECK_LIGHT, CHECK_LIGHT);
  cairo_scale (cr, CHECK_SIZE, CHECK_SIZE);

  pat = get_checkered ();
  cairo_mask (cr, pat);
  cairo_pattern_destroy (pat);

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

  gtk_window_set_default_size (GTK_WINDOW (window), 400, -1);

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
