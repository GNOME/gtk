/* Simplistic test suite */

#include <stdio.h>

#include <gtk/gtk.h>
#include "gtktextbtree.h"

static void fill_buffer(GtkTextBuffer *buffer);

static void run_tests(GtkTextBuffer *buffer);

int
main(int argc, char** argv)
{
  GtkTextBuffer *buffer;
  int n;
  GtkTextUniChar ch;
  GtkTextIter start, end;
  
  gtk_init(&argc, &argv);

  /* Check UTF8 unknown char thing */
  g_assert(gtk_text_view_num_utf_chars(gtk_text_unknown_char_utf8, 3) == 1);
  gtk_text_utf_to_unichar(gtk_text_unknown_char_utf8, &ch);
  g_assert(ch == gtk_text_unknown_char);
  
  /* First, we turn on btree debugging. */
  gtk_debug_flags |= GTK_DEBUG_TEXT;

  /* Create a buffer */
  buffer = gtk_text_buffer_new(NULL);

  /* Check that buffer starts with one empty line and zero chars */
  
  n = gtk_text_buffer_get_line_count(buffer);
  if (n != 1)
    g_error("%d lines, expected 1", n);
  
  n = gtk_text_buffer_get_char_count(buffer);
  if (n != 1)
    g_error("%d chars, expected 1", n);
  
  /* Run gruesome alien test suite on buffer */
  run_tests(buffer);
  
  /* Put stuff in the buffer */

  fill_buffer(buffer);

  /* Subject stuff-bloated buffer to further torment */
  run_tests(buffer);

  /* Delete all stuff from the buffer */
  gtk_text_buffer_get_bounds(buffer, &start, &end);
  gtk_text_buffer_delete(buffer, &start, &end);

  /* Check buffer for emptiness (note that a single
     empty line always remains in the buffer) */
  n = gtk_text_buffer_get_line_count(buffer);
  if (n != 1)
    g_error("%d lines, expected 1", n);
  
  n = gtk_text_buffer_get_char_count(buffer);
  if (n != 1)
    g_error("%d chars, expected 1", n);

  run_tests(buffer);
  
  return 0;
}

static void
run_tests(GtkTextBuffer *buffer)
{
  GtkTextIter iter;
  GtkTextIter start;
  GtkTextIter end;
  GtkTextIter mark;
  gint i, j;
  gint num_chars;
  GtkTextMark *bar_mark;
  
  gtk_text_buffer_get_bounds(buffer, &start, &end);
  
  /* Check that walking the tree via chars and via indexes produces
   * the same number of indexable locations.
   */
  num_chars = gtk_text_buffer_get_char_count(buffer);
  iter = start;
  bar_mark = gtk_text_buffer_create_mark(buffer, "bar", &iter, FALSE);
  i = 0;
  while (i < num_chars)
    {
      GtkTextIter current;
      GtkTextMark *foo_mark;
      
      gtk_text_buffer_get_iter_at_char(buffer, &current, i);

      if (!gtk_text_iter_equal(&iter, &current))
        {
          g_error("get_char_index didn't return current iter");
        }

      j = gtk_text_iter_get_char_index(&iter);

      if (i != j)
        {
          g_error("iter converted to %d not %d", j, i);
        }

      /* get/set mark */
      gtk_text_buffer_get_iter_at_mark(buffer, &mark, bar_mark);

      if (!gtk_text_iter_equal(&iter, &mark))
        {
          gtk_text_iter_spew(&iter, "iter");
          gtk_text_iter_spew(&mark, "mark");
          g_error("Mark not moved to the right place.");
        }
      
      foo_mark = gtk_text_buffer_create_mark(buffer, "foo", &iter, FALSE);
      gtk_text_buffer_get_iter_at_mark(buffer, &mark, foo_mark);
      gtk_text_buffer_delete_mark(buffer, foo_mark);
      
      if (!gtk_text_iter_equal(&iter, &mark))
        {
          gtk_text_iter_spew(&iter, "iter");
          gtk_text_iter_spew(&mark, "mark");
          g_error("Mark not created in the right place.");
        }
      
      if (!gtk_text_iter_forward_char(&iter))
        g_error("iterators ran out before chars");      

      gtk_text_buffer_move_mark(buffer, bar_mark, &iter);
      
      ++i;
    }

  if (!gtk_text_iter_equal(&iter, &end))
    g_error("Iterating over all chars didn't end with the end iter");

  /* Do the tree-walk backward 
   */
  num_chars = gtk_text_buffer_get_char_count(buffer);
  gtk_text_buffer_get_iter_at_char(buffer, &iter, -1);

  gtk_text_buffer_move_mark(buffer, bar_mark, &iter);
  
  i = num_chars;

  if (!gtk_text_iter_equal(&iter, &end))
    g_error("iter at char -1 is not equal to the end iterator");
  
  while (i >= 0)
    {
      GtkTextIter current;
      GtkTextMark *foo_mark;
      
      gtk_text_buffer_get_iter_at_char(buffer, &current, i);

      if (!gtk_text_iter_equal(&iter, &current))
        {
          g_error("get_char_index didn't return current iter while going backward");
        }
      j = gtk_text_iter_get_char_index(&iter);

      if (i != j)
        {
          g_error("going backward, iter converted to %d not %d", j, i);
        }

      /* get/set mark */
      gtk_text_buffer_get_iter_at_mark(buffer, &mark, bar_mark);

      if (!gtk_text_iter_equal(&iter, &mark))
        {
          gtk_text_iter_spew(&iter, "iter");
          gtk_text_iter_spew(&mark, "mark");
          g_error("Mark not moved to the right place.");
        }
      
      foo_mark = gtk_text_buffer_create_mark(buffer, "foo", &iter, FALSE);
      gtk_text_buffer_get_iter_at_mark(buffer, &mark, foo_mark);
      gtk_text_buffer_delete_mark(buffer, foo_mark);
      
      if (!gtk_text_iter_equal(&iter, &mark))
        {
          gtk_text_iter_spew(&iter, "iter");
          gtk_text_iter_spew(&mark, "mark");
          g_error("Mark not created in the right place.");
        }
      
      if (i > 0)
        {
          if (!gtk_text_iter_backward_char(&iter))
            g_error("iterators ran out before char indexes");

          gtk_text_buffer_move_mark(buffer, bar_mark, &iter);
        }
      else
        {
          if (gtk_text_iter_backward_char(&iter))
            g_error("went backward from 0?");
        }
      
      --i;
    }
  
  if (!gtk_text_iter_equal(&iter, &start))
    g_error("Iterating backward over all chars didn't end with the start iter");

  /*
   * Check that get_line_count returns the same number of lines
   * as walking the tree by line
   */
  i = 1; /* include current (first) line */
  gtk_text_buffer_get_iter_at_line(buffer, &iter, 0);
  while (gtk_text_iter_forward_line(&iter))
    ++i;

  /* Add 1 to the line count, because 'i' counts the end-iterator line */
  if (i != gtk_text_buffer_get_line_count(buffer) + 1)
    g_error("Counted %d lines, buffer has %d", i,
            gtk_text_buffer_get_line_count(buffer) + 1);
}


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
"                "};

static void
fill_buffer(GtkTextBuffer *buffer)
{
  GtkTextTag *tag;
  GdkColor color, color2;
  GtkTextIter iter;
  GtkTextIter iter2;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  int i;
  
  tag = gtk_text_buffer_create_tag(buffer, "fg_blue");

  color.red = color.green = 0;
  color.blue = 0xffff;
  color2.red = 0xfff;
  color2.blue = 0x0;
  color2.green = 0;
  gtk_object_set(GTK_OBJECT(tag),
                 "foreground_gdk", &color,
                 "background_gdk", &color2,
                 "font", "-*-courier-bold-r-*-*-30-*-*-*-*-*-*-*",
                 NULL);

  tag = gtk_text_buffer_create_tag(buffer, "fg_red");

  color.blue = color.green = 0;
  color.red = 0xffff;
  gtk_object_set(GTK_OBJECT(tag),
                 "offset", -4,
                 "foreground_gdk", &color,
                 NULL);

  tag = gtk_text_buffer_create_tag(buffer, "bg_green");

  color.blue = color.red = 0;
  color.green = 0xffff;
  gtk_object_set(GTK_OBJECT(tag),
                 "background_gdk", &color,
                 "font", "-*-courier-bold-r-*-*-10-*-*-*-*-*-*-*",
                 NULL);

  pixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL,
                                                  gtk_widget_get_default_colormap(),
                                                  &mask,
                                                  NULL, book_closed_xpm);
  
  g_assert(pixmap != NULL);
  
  i = 0;
  while (i < 10)
    {
      gchar *str;

      gtk_text_buffer_get_iter_at_char(buffer, &iter, 0);
      
      gtk_text_buffer_insert_pixmap (buffer, &iter, pixmap, mask);

      gtk_text_buffer_get_iter_at_char(buffer, &iter, 1);
      
      gtk_text_buffer_insert_pixmap (buffer, &iter, pixmap, mask);
      
      str = g_strdup_printf("%d Hello World!\nwoo woo woo woo woo woo woo woo\n",
                            i);
      
      gtk_text_buffer_insert(buffer, &iter, str, -1);

      g_free(str);
      
      gtk_text_buffer_insert(buffer, &iter,
                              "(Hello World!)\nfoo foo Hello this is some text we are using to text word wrap. It has punctuation! gee; blah - hmm, great.\nnew line\n\n"
                              /* This is UTF8 stuff, Emacs doesn't
                                 really know how to display it */
                              "Spanish (Español) ¡Hola! / French (Français) Bonjour, Salut / German (Deutsch Süd) Grüß Gott (testing Latin-1 chars encoded in UTF8)\nThai (we can't display this, just making sure we don't crash)  (ภาษาไทย)  สวัสดีครับ, สวัสดีค่ะ\n",
                              -1);  
      
      gtk_text_buffer_insert_pixmap (buffer, &iter, pixmap, mask);
      gtk_text_buffer_insert_pixmap (buffer, &iter, pixmap, mask);
      
      gtk_text_buffer_get_iter_at_char(buffer, &iter, 4);
      
      gtk_text_buffer_insert_pixmap (buffer, &iter, pixmap, mask);

      gtk_text_buffer_get_iter_at_char(buffer, &iter, 7);
      
      gtk_text_buffer_insert_pixmap (buffer, &iter, pixmap, mask);

      gtk_text_buffer_get_iter_at_char(buffer, &iter, 8);
      
      gtk_text_buffer_insert_pixmap (buffer, &iter, pixmap, mask);

      gtk_text_buffer_get_iter_at_line_char(buffer, &iter, 0, 8);
      iter2 = iter;
      gtk_text_iter_forward_chars(&iter2, 10);

      gtk_text_buffer_apply_tag(buffer, "fg_blue", &iter, &iter2);

      gtk_text_iter_forward_chars(&iter, 7);
      gtk_text_iter_forward_chars(&iter2, 10);
      
      gtk_text_buffer_apply_tag(buffer, "bg_green", &iter, &iter2);

      gtk_text_iter_forward_chars(&iter, 12);
      gtk_text_iter_forward_chars(&iter2, 10);
      
      gtk_text_buffer_apply_tag(buffer, "bg_green", &iter, &iter2);

      gtk_text_iter_forward_chars(&iter, 10);
      gtk_text_iter_forward_chars(&iter2, 15);
      
      gtk_text_buffer_apply_tag(buffer, "fg_red", &iter, &iter2);
      gtk_text_buffer_apply_tag(buffer, "fg_blue", &iter, &iter2);      

      gtk_text_iter_forward_chars(&iter, 20);
      gtk_text_iter_forward_chars(&iter2, 20);
      
      gtk_text_buffer_apply_tag(buffer, "fg_red", &iter, &iter2);
      gtk_text_buffer_apply_tag(buffer, "fg_blue", &iter, &iter2);      

      gtk_text_iter_backward_chars(&iter, 25);
      gtk_text_iter_forward_chars(&iter2, 5);
      
      gtk_text_buffer_apply_tag(buffer, "fg_red", &iter, &iter2);
      gtk_text_buffer_apply_tag(buffer, "fg_blue", &iter, &iter2);      

      gtk_text_iter_forward_chars(&iter, 15);
      gtk_text_iter_backward_chars(&iter2, 10);

      gtk_text_buffer_remove_tag(buffer, "fg_red", &iter, &iter2);
      gtk_text_buffer_remove_tag(buffer, "fg_blue", &iter, &iter2);      
      
      ++i;
    }

  gdk_pixmap_unref(pixmap);
  if (mask)
    gdk_bitmap_unref(mask);
}


