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
static char * book_open_xpm[] = {
"16 16 4 1",
"       c None s None",
".      c black",
"X      c #808080",
"o      c white",
"                ",
"  ..            ",
" .Xo.    ...    ",
" .Xoo. ..oo.    ",
" .Xooo.Xooo...  ",
" .Xooo.oooo.X.  ",
" .Xooo.Xooo.X.  ",
" .Xooo.oooo.X.  ",
" .Xooo.Xooo.X.  ",
" .Xooo.oooo.X.  ",
"  .Xoo.Xoo..X.  ",
"   .Xo.o..ooX.  ",
"    .X..XXXXX.  ",
"    ..X.......  ",
"     ..         ",
"                "};

#define gray50_width 2
#define gray50_height 2
static char gray50_bits[] = {
  0x02, 0x01
};

/* FIXME once TextTag is a GObject remove these awful hacks ;-) */
#define g_object_set gtk_object_set
#undef G_OBJECT
#define G_OBJECT(x) ((void*)x)

static void
create_tags (GtkTextBuffer *buffer)
{
  GtkTextTag *tag;
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
   * tag table.  That is, tags created later that affect the same
   * text property as an earlier tag will override the earlier tag.
   * You can modify tag priorities with gtk_text_tag_set_priority().
   */

  tag = gtk_text_buffer_create_tag (buffer, "italic");
  g_object_set (G_OBJECT (tag), "font", "sans italic", NULL);

  tag = gtk_text_buffer_create_tag (buffer, "bold");
  g_object_set (G_OBJECT (tag), "font", "sans bold", NULL);  

  tag = gtk_text_buffer_create_tag (buffer, "big");
  g_object_set (G_OBJECT (tag), "font", "sans 120", NULL);
  
  tag = gtk_text_buffer_create_tag (buffer, "blue_foreground");
  g_object_set (G_OBJECT (tag), "foreground", "blue", NULL);  

  tag = gtk_text_buffer_create_tag (buffer, "red_background");
  g_object_set (G_OBJECT (tag), "background", "red", NULL);

  stipple = gdk_bitmap_create_from_data (NULL,
                                         gray50_bits, gray50_width,
                                         gray50_height);
  
  tag = gtk_text_buffer_create_tag (buffer, "background_stipple");
  g_object_set (G_OBJECT (tag), "background_stipple", stipple, NULL);

  tag = gtk_text_buffer_create_tag (buffer, "foreground_stipple");
  g_object_set (G_OBJECT (tag), "foreground_stipple", stipple, NULL);

  g_object_unref (G_OBJECT (stipple));

  tag = gtk_text_buffer_create_tag (buffer, "big_gap_before_line");
  g_object_set (G_OBJECT (tag), "pixels_above_lines", 30, NULL);

  tag = gtk_text_buffer_create_tag (buffer, "big_gap_after_line");
  g_object_set (G_OBJECT (tag), "pixels_below_lines", 30, NULL);

  tag = gtk_text_buffer_create_tag (buffer, "double_spaced_line");
  g_object_set (G_OBJECT (tag), "pixels_inside_wrap", 10, NULL);

  tag = gtk_text_buffer_create_tag (buffer, "not_editable");
  g_object_set (G_OBJECT (tag), "editable", FALSE, NULL);
  
  tag = gtk_text_buffer_create_tag (buffer, "word_wrap");
  g_object_set (G_OBJECT (tag), "wrap_mode", GTK_WRAPMODE_WORD, NULL);

  tag = gtk_text_buffer_create_tag (buffer, "char_wrap");
  g_object_set (G_OBJECT (tag), "wrap_mode", GTK_WRAPMODE_CHAR, NULL);

  tag = gtk_text_buffer_create_tag (buffer, "no_wrap");
  g_object_set (G_OBJECT (tag), "wrap_mode", GTK_WRAPMODE_NONE, NULL);
  
  tag = gtk_text_buffer_create_tag (buffer, "center");
  g_object_set (G_OBJECT (tag), "justify", GTK_JUSTIFY_CENTER, NULL);

  tag = gtk_text_buffer_create_tag (buffer, "right_justify");
  g_object_set (G_OBJECT (tag), "justify", GTK_JUSTIFY_RIGHT, NULL);

  tag = gtk_text_buffer_create_tag (buffer, "wide_margins");
  g_object_set (G_OBJECT (tag),
                "left_margin", 50, "right_margin", 50,
                NULL);

  tag = gtk_text_buffer_create_tag (buffer, "strikethrough");
  g_object_set (G_OBJECT (tag), "strikethrough", TRUE, NULL);
  
  tag = gtk_text_buffer_create_tag (buffer, "underline");
  g_object_set (G_OBJECT (tag), "underline", PANGO_UNDERLINE_SINGLE, NULL);

  tag = gtk_text_buffer_create_tag (buffer, "double_underline");
  g_object_set (G_OBJECT (tag), "underline", PANGO_UNDERLINE_DOUBLE, NULL);

  tag = gtk_text_buffer_create_tag (buffer, "superscript");
  g_object_set (G_OBJECT (tag),
                "offset", 10,
                "font", "Sans 8",
                NULL);
  
  tag = gtk_text_buffer_create_tag (buffer, "subscript");
  g_object_set (G_OBJECT (tag),
                "offset", -10,
                "font", "Sans 8",
                NULL); 
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

  gtk_text_buffer_insert (buffer, &iter, "The text widget can display text with all kinds of nifty attributes.\n", -1);

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
                                            "huge", -1,
                                            "big", NULL);
  gtk_text_buffer_insert (buffer, &iter, " text. ", -1);

  gtk_text_buffer_insert (buffer, &iter, "Also, colors such as ", -1);  
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
  gtk_text_buffer_insert (buffer, &iter, " can be used.\n", -1);  

  
  
  
  gtk_text_buffer_insert (buffer, &iter, "This demo doesn't even demonstrate all the GtkTextBuffer features; it leaves out, for example: invisible/hidden text, tab stops, application-drawn areas on the sides of the widget for displaying breakpoints and such...", -1);

  /* Apply word_wrap tag to whole buffer */
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  gtk_text_buffer_apply_tag_by_name (buffer, "word_wrap", &start, &end);
}

void
do_textview (void)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *vpaned;
      GtkWidget *view1;
      GtkWidget *view2;
      GtkWidget *frame;
      GtkTextBuffer *buffer;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

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
      
      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_usize (frame, 300, 200);
      gtk_paned_add1 (GTK_PANED (vpaned), frame);

      gtk_container_add (GTK_CONTAINER (frame), view1);
      
      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_usize (frame, 300, 200);
      gtk_paned_add2 (GTK_PANED (vpaned), frame);

      gtk_container_add (GTK_CONTAINER (frame), view2);

      create_tags (buffer);
      insert_text (buffer);
      
      gtk_widget_show_all (vpaned);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

