/* Text Widget
 *
 * The GtkTextView widget displays a GtkTextBuffer. One GtkTextBuffer
 * can be displayed by multiple GtkTextViews. This demo has two views
 * displaying a single buffer, and shows off the widget's text
 * formatting features.
 *
 */

#include <gtk/gtk.h>

/* Don't copy this bad example; inline RGB data is always a better
 * idea than inline XPMs.
 */
static char  *book_closed_xpm[] = {
"16 16 6 1",
"       c None s None",
".      c black",
"X      c red",
"o      c yellow",
"O      c #808080",
"#      c white",
"                ",
"       ..       ",
"     ..XX.      ",
"   ..XXXXX.     ",
" ..XXXXXXXX.    ",
".ooXXXXXXXXX.   ",
"..ooXXXXXXXXX.  ",
".X.ooXXXXXXXXX. ",
".XX.ooXXXXXX..  ",
" .XX.ooXXX..#O  ",
"  .XX.oo..##OO. ",
"   .XX..##OO..  ",
"    .X.#OO..    ",
"     ..O..      ",
"      ..        ",
"                "
};


#define gray50_width 2
#define gray50_height 2
static char gray50_bits[] = {
  0x02, 0x01
};

static void
create_tags (GtkTextBuffer *buffer)
{
  GdkBitmap *stipple;

  /* Create a bunch of tags. Note that it's also possible to
   * create tags with gtk_text_tag_new() then add them to the
   * tag table for the buffer, gtk_text_buffer_create_tag() is
   * just a convenience function. Also note that you don't have
   * to give tags a name; pass NULL for the name to create an
   * anonymous tag.
   *
   * In any real app, another useful optimization would be to create
   * a GtkTextTagTable in advance, and reuse the same tag table for
   * all the buffers with the same tag set, instead of creating
   * new copies of the same tags for every buffer.
   *
   * Tags are assigned default priorities in order of addition to the
   * tag table.  That is, tags created later that affect the same text
   * property affected by an earlier tag will override the earlier
   * tag.  You can modify tag priorities with
   * gtk_text_tag_set_priority().
   */

  gtk_text_buffer_create_tag (buffer, "heading",
                              "weight", PANGO_WEIGHT_BOLD,
                              "size", 20 * PANGO_SCALE,
                              NULL);
  
  gtk_text_buffer_create_tag (buffer, "italic",
                              "style", PANGO_STYLE_ITALIC, NULL);

  gtk_text_buffer_create_tag (buffer, "bold",
                              "weight", PANGO_WEIGHT_BOLD, NULL);  
  
  gtk_text_buffer_create_tag (buffer, "big",
                              /* points times the PANGO_SCALE factor */
                              "size", 30 * PANGO_SCALE, NULL);

  gtk_text_buffer_create_tag (buffer, "xx-small",
                              "scale", PANGO_SCALE_XX_SMALL, NULL);

  gtk_text_buffer_create_tag (buffer, "x-large",
                              "scale", PANGO_SCALE_X_LARGE, NULL);
  
  gtk_text_buffer_create_tag (buffer, "monospace",
                              "family", "monospace", NULL);
  
  gtk_text_buffer_create_tag (buffer, "blue_foreground",
                              "foreground", "blue", NULL);  

  gtk_text_buffer_create_tag (buffer, "red_background",
                              "background", "red", NULL);

  stipple = gdk_bitmap_create_from_data (NULL,
                                         gray50_bits, gray50_width,
                                         gray50_height);
  
  gtk_text_buffer_create_tag (buffer, "background_stipple",
                              "background_stipple", stipple, NULL);

  gtk_text_buffer_create_tag (buffer, "foreground_stipple",
                              "foreground_stipple", stipple, NULL);

  g_object_unref (G_OBJECT (stipple));

  gtk_text_buffer_create_tag (buffer, "big_gap_before_line",
                              "pixels_above_lines", 30, NULL);

  gtk_text_buffer_create_tag (buffer, "big_gap_after_line",
                              "pixels_below_lines", 30, NULL);

  gtk_text_buffer_create_tag (buffer, "double_spaced_line",
                              "pixels_inside_wrap", 10, NULL);

  gtk_text_buffer_create_tag (buffer, "not_editable",
                              "editable", FALSE, NULL);
  
  gtk_text_buffer_create_tag (buffer, "word_wrap",
                              "wrap_mode", GTK_WRAP_WORD, NULL);

  gtk_text_buffer_create_tag (buffer, "char_wrap",
                              "wrap_mode", GTK_WRAP_CHAR, NULL);

  gtk_text_buffer_create_tag (buffer, "no_wrap",
                              "wrap_mode", GTK_WRAP_NONE, NULL);
  
  gtk_text_buffer_create_tag (buffer, "center",
                              "justification", GTK_JUSTIFY_CENTER, NULL);

  gtk_text_buffer_create_tag (buffer, "right_justify",
                              "justification", GTK_JUSTIFY_RIGHT, NULL);

  gtk_text_buffer_create_tag (buffer, "wide_margins",
                              "left_margin", 50, "right_margin", 50,
                              NULL);
  
  gtk_text_buffer_create_tag (buffer, "strikethrough",
                              "strikethrough", TRUE, NULL);
  
  gtk_text_buffer_create_tag (buffer, "underline",
                              "underline", PANGO_UNDERLINE_SINGLE, NULL);

  gtk_text_buffer_create_tag (buffer, "double_underline",
                              "underline", PANGO_UNDERLINE_DOUBLE, NULL);

  gtk_text_buffer_create_tag (buffer, "superscript",
                              "rise", 10 * PANGO_SCALE,   /* 10 pixels */
                              "size", 8 * PANGO_SCALE,    /* 8 points */
                              NULL);
  
  gtk_text_buffer_create_tag (buffer, "subscript",
                              "rise", -10 * PANGO_SCALE,   /* 10 pixels */
                              "size", 8 * PANGO_SCALE,     /* 8 points */
                              NULL);

  gtk_text_buffer_create_tag (buffer, "rtl_quote",
                              "wrap_mode", GTK_WRAP_WORD,
                              "direction", GTK_TEXT_DIR_RTL,
                              "indent", 30,
                              "left_margin", 20,
                              "right_margin", 20,
                              NULL);
}

static void
insert_text (GtkTextBuffer *buffer)
{
  GtkTextIter iter;
  GtkTextIter start, end;
  GdkPixbuf *pixbuf;

  pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) book_closed_xpm);
  
  /* get start of buffer; each insertion will revalidate the
   * iterator to point to just after the inserted text.
   */
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);

  gtk_text_buffer_insert (buffer, &iter, "The text widget can display text with all kinds of nifty attributes. It also supports multiple views of the same buffer; this demo is showing the same buffer in two places.\n\n", -1);

  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, "Font styles. ", -1,
                                            "heading", NULL);
  
  gtk_text_buffer_insert (buffer, &iter, "For example, you can have ", -1);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "italic", -1,
                                            "italic", NULL);
  gtk_text_buffer_insert (buffer, &iter, ", ", -1);  
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "bold", -1,
                                            "bold", NULL);
  gtk_text_buffer_insert (buffer, &iter, ", or ", -1);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "monospace (typewriter)", -1,
                                            "monospace", NULL);
  gtk_text_buffer_insert (buffer, &iter, ", or ", -1);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "big", -1,
                                            "big", NULL);
  gtk_text_buffer_insert (buffer, &iter, " text. ", -1);
  gtk_text_buffer_insert (buffer, &iter, "It's best not to hardcode specific text sizes; you can use relative sizes as with CSS, such as ", -1);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "xx-small", -1,
                                            "xx-small", NULL);
  gtk_text_buffer_insert (buffer, &iter, " or ", -1);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "x-large", -1,
                                            "x-large", NULL);
  gtk_text_buffer_insert (buffer, &iter, " to ensure that your program properly adapts if the user changes the default font size.\n\n", -1);
  
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, "Colors. ", -1,
                                            "heading", NULL);
  
  gtk_text_buffer_insert (buffer, &iter, "Colors such as ", -1);  
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "a blue foreground", -1,
                                            "blue_foreground", NULL);
  gtk_text_buffer_insert (buffer, &iter, " or ", -1);  
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "a red background", -1,
                                            "red_background", NULL);
  gtk_text_buffer_insert (buffer, &iter, " or even ", -1);  
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "a stippled red background", -1,
                                            "red_background",
                                            "background_stipple",
                                            NULL);

  gtk_text_buffer_insert (buffer, &iter, " or ", -1);  
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "a stippled blue foreground on solid red background", -1,
                                            "blue_foreground",
                                            "red_background",
                                            "foreground_stipple",
                                            NULL);
  gtk_text_buffer_insert (buffer, &iter, " (select that to read it) can be used.\n\n", -1);  

  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, "Underline, strikethrough, and rise. ", -1,
                                            "heading", NULL);
  
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "Strikethrough", -1,
                                            "strikethrough", NULL);
  gtk_text_buffer_insert (buffer, &iter, ", ", -1);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "underline", -1,
                                            "underline", NULL);
  gtk_text_buffer_insert (buffer, &iter, ", ", -1);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "double underline", -1, 
                                            "double_underline", NULL);
  gtk_text_buffer_insert (buffer, &iter, ", ", -1);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "superscript", -1,
                                            "superscript", NULL);
  gtk_text_buffer_insert (buffer, &iter, ", and ", -1);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "subscript", -1,
                                            "subscript", NULL);
  gtk_text_buffer_insert (buffer, &iter, " are all supported.\n\n", -1);

  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, "Images. ", -1,
                                            "heading", NULL);
  
  gtk_text_buffer_insert (buffer, &iter, "The buffer can have images in it: ", -1);
  gtk_text_buffer_insert_pixbuf (buffer, &iter, pixbuf);
  gtk_text_buffer_insert_pixbuf (buffer, &iter, pixbuf);
  gtk_text_buffer_insert_pixbuf (buffer, &iter, pixbuf);
  gtk_text_buffer_insert (buffer, &iter, " for example.\n\n", -1);

  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, "Spacing. ", -1,
                                            "heading", NULL);

  gtk_text_buffer_insert (buffer, &iter, "You can adjust the amount of space before each line.\n", -1);
  
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "This line has a whole lot of space before it.\n", -1,
                                            "big_gap_before_line", "wide_margins", NULL);
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "You can also adjust the amount of space after each line; this line has a whole lot of space after it.\n", -1,
                                            "big_gap_after_line", "wide_margins", NULL);
  
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "You can also adjust the amount of space between wrapped lines; this line has extra space between each wrapped line in the same paragraph. To show off wrapping, some filler text: the quick brown fox jumped over the lazy dog. Blah blah blah blah blah blah blah blah blah.\n", -1,
                                            "double_spaced_line", "wide_margins", NULL);

  gtk_text_buffer_insert (buffer, &iter, "Also note that those lines have extra-wide margins.\n\n", -1);

  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, "Editability. ", -1,
                                            "heading", NULL);
  
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "This line is 'locked down' and can't be edited by the user - just try it! You can't delete this line.\n\n", -1,
                                            "not_editable", NULL);

  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, "Wrapping. ", -1,
                                            "heading", NULL);

  gtk_text_buffer_insert (buffer, &iter,
                          "This line (and most of the others in this buffer) is word-wrapped, using the proper Unicode algorithm. Word wrap should work in all scripts and languages that GTK+ supports. Let's make this a long paragraph to demonstrate: blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah\n\n", -1);  
  
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "This line has character-based wrapping, and can wrap between any two character glyphs. Let's make this a long paragraph to demonstrate: blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah\n\n", -1,
                                            "char_wrap", NULL);
  
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "This line has all wrapping turned off, so it makes the horizontal scrollbar appear.\n\n\n", -1,
                                            "no_wrap", NULL);

  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, "Justification. ", -1,
                                            "heading", NULL);  
  
  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "\nThis line has center justification.\n", -1,
                                            "center", NULL);

  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "This line has right justification.\n", -1,
                                            "right_justify", NULL);

  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
                                            "\nThis line has big wide margins. Text text text text text text text text text text text text text text text text text text text text text text text text text text text text text text text text text text text text.\n", -1,
                                            "wide_margins", NULL);  

  gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, "Internationalization. ", -1,
                                            "heading", NULL);
          
  gtk_text_buffer_insert (buffer, &iter,
                          "You can put all sorts of Unicode text in the buffer.\n\nGerman (Deutsch Süd) Grüß Gott\nGreek (Ελληνικά) Γειά σας\nHebrew   שלום\nJapanese (日本語)\n\nThe widget properly handles bidirectional text, word wrapping, DOS/UNIX/Unicode paragraph separators, grapheme boundaries, and so on using the Pango internationalization framework.\n", -1);  

      gtk_text_buffer_insert (buffer, &iter, "Here's a word-wrapped quote in a right-to-left language:\n", -1);
      gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, "وقد بدأ ثلاث من أكثر المؤسسات تقدما في شبكة اكسيون برامجها كمنظمات لا تسعى للربح، ثم تحولت في السنوات الخمس الماضية إلى مؤسسات مالية منظمة، وباتت جزءا من النظام المالي في بلدانها، ولكنها تتخصص في خدمة قطاع المشروعات الصغيرة. وأحد أكثر هذه المؤسسات نجاحا هو »بانكوسول« في بوليفيا.\n\n", -1,
                                                "rtl_quote", NULL);
  
  gtk_text_buffer_insert (buffer, &iter, "\n\nThis demo doesn't demonstrate all the GtkTextBuffer features; it leaves out, for example: invisible/hidden text (doesn't work in GTK 2, but planned), tab stops, application-drawn areas on the sides of the widget for displaying breakpoints and such...", -1);

  /* Apply word_wrap tag to whole buffer */
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  gtk_text_buffer_apply_tag_by_name (buffer, "word_wrap", &start, &end);

  g_object_unref (G_OBJECT (pixbuf));
}

GtkWidget *
do_textview (void)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *vpaned;
      GtkWidget *view1;
      GtkWidget *view2;
      GtkWidget *sw;
      GtkTextBuffer *buffer;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_default_size (GTK_WINDOW (window),
                                   450, 450);
      
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "TextView");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      vpaned = gtk_vpaned_new ();
      gtk_container_set_border_width (GTK_CONTAINER(vpaned), 5);
      gtk_container_add (GTK_CONTAINER (window), vpaned);

      /* For convenience, we just use the autocreated buffer from
       * the first text view; you could also create the buffer
       * by itself with gtk_text_buffer_new(), then later create
       * a view widget.
       */
      view1 = gtk_text_view_new ();
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view1));
      view2 = gtk_text_view_new_with_buffer (buffer);
      
      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_paned_add1 (GTK_PANED (vpaned), sw);

      gtk_container_add (GTK_CONTAINER (sw), view1);

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_paned_add2 (GTK_PANED (vpaned), sw);

      gtk_container_add (GTK_CONTAINER (sw), view2);

      create_tags (buffer);
      insert_text (buffer);
      
      gtk_widget_show_all (vpaned);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    {
      gtk_widget_show (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}

