/* Simplistic test suite */


#include <stdio.h>

#include <gtk/gtk.h>
#include "../gtk/gtktexttypes.h" /* Private header, for UNKNOWN_CHAR */

static void
gtk_text_iter_spew (const GtkTextIter *iter, const gchar *desc)
{
  g_print (" %20s: line %d / char %d / line char %d / line byte %d\n",
           desc,
           gtk_text_iter_get_line (iter),
           gtk_text_iter_get_offset (iter),
           gtk_text_iter_get_line_offset (iter),
           gtk_text_iter_get_line_index (iter));
}

static void fill_buffer (GtkTextBuffer *buffer);

static void run_tests (GtkTextBuffer *buffer);

int
main (int argc, char** argv)
{
  GtkTextBuffer *buffer;
  int n;
  gunichar ch;
  GtkTextIter start, end;

  gtk_init (&argc, &argv);

  /* Check UTF8 unknown char thing */
  g_assert (g_utf8_strlen (gtk_text_unknown_char_utf8, 3) == 1);
  ch = g_utf8_get_char (gtk_text_unknown_char_utf8);
  g_assert (ch == GTK_TEXT_UNKNOWN_CHAR);

  /* First, we turn on btree debugging. */
  gtk_debug_flags |= GTK_DEBUG_TEXT;

  /* Create a buffer */
  buffer = gtk_text_buffer_new (NULL);

  /* Check that buffer starts with one empty line and zero chars */

  n = gtk_text_buffer_get_line_count (buffer);
  if (n != 1)
    g_error ("%d lines, expected 1", n);

  n = gtk_text_buffer_get_char_count (buffer);
  if (n != 1)
    g_error ("%d chars, expected 1", n);

  /* Run gruesome alien test suite on buffer */
  run_tests (buffer);

  /* Put stuff in the buffer */

  fill_buffer (buffer);

  /* Subject stuff-bloated buffer to further torment */
  run_tests (buffer);

  /* Delete all stuff from the buffer */
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  gtk_text_buffer_delete (buffer, &start, &end);

  /* Check buffer for emptiness (note that a single
     empty line always remains in the buffer) */
  n = gtk_text_buffer_get_line_count (buffer);
  if (n != 1)
    g_error ("%d lines, expected 1", n);

  n = gtk_text_buffer_get_char_count (buffer);
  if (n != 1)
    g_error ("%d chars, expected 1", n);

  run_tests (buffer);

  g_object_unref (G_OBJECT (buffer));
  
  g_print ("All tests passed.\n");

  return 0;
}

static gint
count_toggles_at_iter (GtkTextIter *iter,
                       GtkTextTag  *of_tag)
{
  GSList *tags;
  GSList *tmp;
  gint count = 0;
  
  /* get toggle-ons and toggle-offs */
  tags = gtk_text_iter_get_toggled_tags (iter, TRUE);
  tags = g_slist_concat (tags,
                         gtk_text_iter_get_toggled_tags (iter, FALSE));
  
  tmp = tags;
  while (tmp != NULL)
    {
      if (of_tag == NULL)
        ++count;
      else if (of_tag == tmp->data)
        ++count;
      
      tmp = g_slist_next (tmp);
    }
  
  g_slist_free (tags);

  return count;
}
     
static gint
count_toggles_in_buffer (GtkTextBuffer *buffer,
                         GtkTextTag    *of_tag)
{
  GtkTextIter iter;
  gint count = 0;
  
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
  do
    {
      count += count_toggles_at_iter (&iter, of_tag);
    }
  while (gtk_text_iter_forward_char (&iter));

  /* Do the end iterator, because forward_char won't return TRUE
   * on it.
   */
  count += count_toggles_at_iter (&iter, of_tag);
  
  return count;
}

static void
check_specific_tag (GtkTextBuffer *buffer,
                    const gchar   *tag_name)
{
  GtkTextIter iter;
  GtkTextTag *tag;
  gboolean state;
  gint count;
  gint buffer_count;
  gint last_offset;
  
  tag = gtk_text_tag_table_lookup (gtk_text_buffer_get_tag_table (buffer),
                                   tag_name);

  buffer_count = count_toggles_in_buffer (buffer, tag);
  
  state = FALSE;
  count = 0;

  last_offset = -1;
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
  if (gtk_text_iter_toggles_tag (&iter, tag) ||
      gtk_text_iter_forward_to_tag_toggle (&iter, tag))
    {
      do
        {
          gint this_offset;
          
          ++count;

          this_offset = gtk_text_iter_get_offset (&iter);

          if (this_offset <= last_offset)
            g_error ("forward_to_tag_toggle moved in wrong direction");

          last_offset = this_offset;
          
          if (gtk_text_iter_begins_tag (&iter, tag))
            {
              if (state)
                g_error ("Tag %p is already on, and was toggled on?", tag);
              state = TRUE;
            }          
          else if (gtk_text_iter_ends_tag (&iter, tag))
            {
              if (!state)
                g_error ("Tag %p toggled off, but wasn't toggled on?", tag);
              state = FALSE;
            }
          else
            g_error ("forward_to_tag_toggle went to a location without a toggle");
        }
      while (gtk_text_iter_forward_to_tag_toggle (&iter, tag));
    }

  if (count != buffer_count)
    g_error ("Counted %d tags iterating by char, %d iterating by tag toggle\n",
             buffer_count, count);
  
  state = FALSE;
  count = 0;
  
  gtk_text_buffer_get_end_iter (buffer, &iter);
  last_offset = gtk_text_iter_get_offset (&iter);
  if (gtk_text_iter_toggles_tag (&iter, tag) ||
      gtk_text_iter_backward_to_tag_toggle (&iter, tag))
    {
      do
        {
          gint this_offset;
          
          ++count;

          this_offset = gtk_text_iter_get_offset (&iter);
          
          if (this_offset >= last_offset)
            g_error ("backward_to_tag_toggle moved in wrong direction");
          
          last_offset = this_offset;

          if (gtk_text_iter_begins_tag (&iter, tag))
            {
              if (!state)
                g_error ("Tag %p wasn't on when we got to the on toggle going backward?", tag);
              state = FALSE;
            }
          else if (gtk_text_iter_ends_tag (&iter, tag))
            {
              if (state)
                g_error ("Tag %p off toggle, but we were already inside a tag?", tag);
              state = TRUE;
            }
          else
            g_error ("backward_to_tag_toggle went to a location without a toggle");
        }
      while (gtk_text_iter_backward_to_tag_toggle (&iter, tag));
    }

  if (count != buffer_count)
    g_error ("Counted %d tags iterating by char, %d iterating by tag toggle\n",
             buffer_count, count);

}

static void
run_tests (GtkTextBuffer *buffer)
{
  GtkTextIter iter;
  GtkTextIter start;
  GtkTextIter end;
  GtkTextIter mark;
  gint i, j;
  gint num_chars;
  GtkTextMark *bar_mark;
  GtkTextTag *tag;
  GHashTable *tag_states;
  gint count;
  gint buffer_count;
  
  gtk_text_buffer_get_bounds (buffer, &start, &end);

  /* Check that walking the tree via chars and via iterators produces
   * the same number of indexable locations.
   */
  num_chars = gtk_text_buffer_get_char_count (buffer);
  iter = start;
  bar_mark = gtk_text_buffer_create_mark (buffer, "bar", &iter, FALSE);
  i = 0;
  while (i < num_chars)
    {
      GtkTextIter current;
      GtkTextMark *foo_mark;

      gtk_text_buffer_get_iter_at_offset (buffer, &current, i);

      if (!gtk_text_iter_equal (&iter, &current))
        {
          g_error ("get_char_index didn't return current iter");
        }

      j = gtk_text_iter_get_offset (&iter);

      if (i != j)
        {
          g_error ("iter converted to %d not %d", j, i);
        }

      /* get/set mark */
      gtk_text_buffer_get_iter_at_mark (buffer, &mark, bar_mark);

      if (!gtk_text_iter_equal (&iter, &mark))
        {
          gtk_text_iter_spew (&iter, "iter");
          gtk_text_iter_spew (&mark, "mark");
          g_error ("Mark not moved to the right place.");
        }

      foo_mark = gtk_text_buffer_create_mark (buffer, "foo", &iter, FALSE);
      gtk_text_buffer_get_iter_at_mark (buffer, &mark, foo_mark);
      gtk_text_buffer_delete_mark (buffer, foo_mark);

      if (!gtk_text_iter_equal (&iter, &mark))
        {
          gtk_text_iter_spew (&iter, "iter");
          gtk_text_iter_spew (&mark, "mark");
          g_error ("Mark not created in the right place.");
        }

      if (gtk_text_iter_is_end (&iter))
        g_error ("iterators ran out before chars (offset %d of %d)",
                 i, num_chars);

      gtk_text_iter_forward_char (&iter);

      gtk_text_buffer_move_mark (buffer, bar_mark, &iter);

      ++i;
    }

  if (!gtk_text_iter_equal (&iter, &end))
    g_error ("Iterating over all chars didn't end with the end iter");

  /* Do the tree-walk backward
   */
  num_chars = gtk_text_buffer_get_char_count (buffer);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);

  gtk_text_buffer_move_mark (buffer, bar_mark, &iter);

  i = num_chars;

  if (!gtk_text_iter_equal (&iter, &end))
    g_error ("iter at char -1 is not equal to the end iterator");

  while (i >= 0)
    {
      GtkTextIter current;
      GtkTextMark *foo_mark;

      gtk_text_buffer_get_iter_at_offset (buffer, &current, i);

      if (!gtk_text_iter_equal (&iter, &current))
        {
          g_error ("get_char_index didn't return current iter while going backward");
        }
      j = gtk_text_iter_get_offset (&iter);

      if (i != j)
        {
          g_error ("going backward, iter converted to %d not %d", j, i);
        }

      /* get/set mark */
      gtk_text_buffer_get_iter_at_mark (buffer, &mark, bar_mark);

      if (!gtk_text_iter_equal (&iter, &mark))
        {
          gtk_text_iter_spew (&iter, "iter");
          gtk_text_iter_spew (&mark, "mark");
          g_error ("Mark not moved to the right place.");
        }

      foo_mark = gtk_text_buffer_create_mark (buffer, "foo", &iter, FALSE);
      gtk_text_buffer_get_iter_at_mark (buffer, &mark, foo_mark);
      gtk_text_buffer_delete_mark (buffer, foo_mark);

      if (!gtk_text_iter_equal (&iter, &mark))
        {
          gtk_text_iter_spew (&iter, "iter");
          gtk_text_iter_spew (&mark, "mark");
          g_error ("Mark not created in the right place.");
        }

      if (i > 0)
        {
          if (!gtk_text_iter_backward_char (&iter))
            g_error ("iterators ran out before char indexes");

          gtk_text_buffer_move_mark (buffer, bar_mark, &iter);
        }
      else
        {
          if (gtk_text_iter_backward_char (&iter))
            g_error ("went backward from 0?");
        }

      --i;
    }

  if (!gtk_text_iter_equal (&iter, &start))
    g_error ("Iterating backward over all chars didn't end with the start iter");

  /*
   * Check that get_line_count returns the same number of lines
   * as walking the tree by line
   */
  i = 1; /* include current (first) line */
  gtk_text_buffer_get_iter_at_line (buffer, &iter, 0);
  while (gtk_text_iter_forward_line (&iter))
    ++i;

  if (i != gtk_text_buffer_get_line_count (buffer))
    g_error ("Counted %d lines, buffer has %d", i,
             gtk_text_buffer_get_line_count (buffer));

  /*
   * Check that moving over tag toggles thinks about working.
   */

  buffer_count = count_toggles_in_buffer (buffer, NULL);
  
  tag_states = g_hash_table_new (NULL, NULL);
  count = 0;
  
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
  if (gtk_text_iter_toggles_tag (&iter, NULL) ||
      gtk_text_iter_forward_to_tag_toggle (&iter, NULL))
    {
      do
        {
          GSList *tags;
          GSList *tmp;
          gboolean found_some = FALSE;
          
          /* get toggled-on tags */
          tags = gtk_text_iter_get_toggled_tags (&iter, TRUE);

          if (tags)
            found_some = TRUE;
          
          tmp = tags;
          while (tmp != NULL)
            {
              ++count;
              
              tag = tmp->data;
              
              if (g_hash_table_lookup (tag_states, tag))
                g_error ("Tag %p is already on, and was toggled on?", tag);

              g_hash_table_insert (tag_states, tag, GINT_TO_POINTER (TRUE));
          
              tmp = g_slist_next (tmp);
            }

          g_slist_free (tags);
      
          /* get toggled-off tags */
          tags = gtk_text_iter_get_toggled_tags (&iter, FALSE);

          if (tags)
            found_some = TRUE;
          
          tmp = tags;
          while (tmp != NULL)
            {
              ++count;
              
              tag = tmp->data;

              if (!g_hash_table_lookup (tag_states, tag))
                g_error ("Tag %p is already off, and was toggled off?", tag);

              g_hash_table_remove (tag_states, tag);
          
              tmp = g_slist_next (tmp);
            }

          g_slist_free (tags);

          if (!found_some)
            g_error ("No tags found going forward to tag toggle.");

        }
      while (gtk_text_iter_forward_to_tag_toggle (&iter, NULL));
    }
  
  g_hash_table_destroy (tag_states);

  if (count != buffer_count)
    g_error ("Counted %d tags iterating by char, %d iterating by tag toggle\n",
             buffer_count, count);
  
  /* Go backward; here TRUE in the hash means we saw
   * an off toggle last.
   */
  
  tag_states = g_hash_table_new (NULL, NULL);
  count = 0;
  
  gtk_text_buffer_get_end_iter (buffer, &iter);
  if (gtk_text_iter_toggles_tag (&iter, NULL) ||
      gtk_text_iter_backward_to_tag_toggle (&iter, NULL))
    {
      do
        {
          GSList *tags;
          GSList *tmp;
          gboolean found_some = FALSE;
          
          /* get toggled-off tags */
          tags = gtk_text_iter_get_toggled_tags (&iter, FALSE);

          if (tags)
            found_some = TRUE;
          
          tmp = tags;
          while (tmp != NULL)
            {
              ++count;
              
              tag = tmp->data;

              if (g_hash_table_lookup (tag_states, tag))
                g_error ("Tag %p has two off-toggles in a row?", tag);
          
              g_hash_table_insert (tag_states, tag, GINT_TO_POINTER (TRUE));
          
              tmp = g_slist_next (tmp);
            }

          g_slist_free (tags);
      
          /* get toggled-on tags */
          tags = gtk_text_iter_get_toggled_tags (&iter, TRUE);

          if (tags)
            found_some = TRUE;
          
          tmp = tags;
          while (tmp != NULL)
            {
              ++count;
              
              tag = tmp->data;

              if (!g_hash_table_lookup (tag_states, tag))
                g_error ("Tag %p was toggled on, but saw no off-toggle?", tag);

              g_hash_table_remove (tag_states, tag);
          
              tmp = g_slist_next (tmp);
            }

          g_slist_free (tags);

          if (!found_some)
            g_error ("No tags found going backward to tag toggle.");
        }
      while (gtk_text_iter_backward_to_tag_toggle (&iter, NULL));
    }
  
  g_hash_table_destroy (tag_states);

  if (count != buffer_count)
    g_error ("Counted %d tags iterating by char, %d iterating by tag toggle\n",
             buffer_count, count);

  check_specific_tag (buffer, "fg_red");
  check_specific_tag (buffer, "bg_green");
  check_specific_tag (buffer, "front_tag");
  check_specific_tag (buffer, "center_tag");
  check_specific_tag (buffer, "end_tag");
}


static const char  *book_closed_xpm[] = {
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
fill_buffer (GtkTextBuffer *buffer)
{
  GtkTextTag *tag;
  GdkColor color, color2;
  GtkTextIter iter;
  GtkTextIter iter2;
  GdkPixbuf *pixbuf;
  int i;

  color.red = color.green = 0;
  color.blue = 0xffff;
  color2.red = 0xfff;
  color2.blue = 0x0;
  color2.green = 0;
  
  gtk_text_buffer_create_tag (buffer, "fg_blue",
                              "foreground_gdk", &color,
                              "background_gdk", &color2,
                              "font", "-*-courier-bold-r-*-*-30-*-*-*-*-*-*-*",
                              NULL);

  color.blue = color.green = 0;
  color.red = 0xffff;
  
  gtk_text_buffer_create_tag (buffer, "fg_red",
                              "rise", -4,
                              "foreground_gdk", &color,
                              NULL);

  color.blue = color.red = 0;
  color.green = 0xffff;
  
  gtk_text_buffer_create_tag (buffer, "bg_green",
                              "background_gdk", &color,
                              "font", "-*-courier-bold-r-*-*-10-*-*-*-*-*-*-*",
                              NULL);

  pixbuf = gdk_pixbuf_new_from_xpm_data (book_closed_xpm);

  g_assert (pixbuf != NULL);

  i = 0;
  while (i < 10)
    {
      gchar *str;

      gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);

      gtk_text_buffer_insert_pixbuf (buffer, &iter, pixbuf);

      gtk_text_buffer_get_iter_at_offset (buffer, &iter, 1);

      gtk_text_buffer_insert_pixbuf (buffer, &iter, pixbuf);

      str = g_strdup_printf ("%d Hello World!\nwoo woo woo woo woo woo woo woo\n",
                            i);

      gtk_text_buffer_insert (buffer, &iter, str, -1);

      g_free (str);

      gtk_text_buffer_insert (buffer, &iter,
                              "(Hello World!)\nfoo foo Hello this is some text we are using to text word wrap. It has punctuation! gee; blah - hmm, great.\nnew line\n\n"
                              /* This is UTF8 stuff, Emacs doesn't
                                 really know how to display it */
                              "Spanish (Español) ¡Hola! / French (Français) Bonjour, Salut / German (Deutsch Süd) Grüß Gott (testing Latin-1 chars encoded in UTF8)\nThai (we can't display this, just making sure we don't crash)  (ภาษาไทย)  สวัสดีครับ, สวัสดีค่ะ\n",
                              -1);

      gtk_text_buffer_insert_pixbuf (buffer, &iter, pixbuf);
      gtk_text_buffer_insert_pixbuf (buffer, &iter, pixbuf);

      gtk_text_buffer_get_iter_at_offset (buffer, &iter, 4);

      gtk_text_buffer_insert_pixbuf (buffer, &iter, pixbuf);

      gtk_text_buffer_get_iter_at_offset (buffer, &iter, 7);

      gtk_text_buffer_insert_pixbuf (buffer, &iter, pixbuf);

      gtk_text_buffer_get_iter_at_offset (buffer, &iter, 8);

      gtk_text_buffer_insert_pixbuf (buffer, &iter, pixbuf);

      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 0, 8);
      iter2 = iter;
      gtk_text_iter_forward_chars (&iter2, 10);

      gtk_text_buffer_apply_tag_by_name (buffer, "fg_blue", &iter, &iter2);

      gtk_text_iter_forward_chars (&iter, 7);
      gtk_text_iter_forward_chars (&iter2, 10);

      gtk_text_buffer_apply_tag_by_name (buffer, "bg_green", &iter, &iter2);

      gtk_text_iter_forward_chars (&iter, 12);
      gtk_text_iter_forward_chars (&iter2, 10);

      gtk_text_buffer_apply_tag_by_name (buffer, "bg_green", &iter, &iter2);

      gtk_text_iter_forward_chars (&iter, 10);
      gtk_text_iter_forward_chars (&iter2, 15);

      gtk_text_buffer_apply_tag_by_name (buffer, "fg_red", &iter, &iter2);
      gtk_text_buffer_apply_tag_by_name (buffer, "fg_blue", &iter, &iter2);

      gtk_text_iter_forward_chars (&iter, 20);
      gtk_text_iter_forward_chars (&iter2, 20);

      gtk_text_buffer_apply_tag_by_name (buffer, "fg_red", &iter, &iter2);
      gtk_text_buffer_apply_tag_by_name (buffer, "fg_blue", &iter, &iter2);

      gtk_text_iter_backward_chars (&iter, 25);
      gtk_text_iter_forward_chars (&iter2, 5);

      gtk_text_buffer_apply_tag_by_name (buffer, "fg_red", &iter, &iter2);
      gtk_text_buffer_apply_tag_by_name (buffer, "fg_blue", &iter, &iter2);

      gtk_text_iter_forward_chars (&iter, 15);
      gtk_text_iter_backward_chars (&iter2, 10);

      gtk_text_buffer_remove_tag_by_name (buffer, "fg_red", &iter, &iter2);
      gtk_text_buffer_remove_tag_by_name (buffer, "fg_blue", &iter, &iter2);

      ++i;
    }

  /* Put in tags that are just at the beginning, and just near the end,
   * and just near the middle.
   */
  tag = gtk_text_buffer_create_tag (buffer, "front_tag", NULL);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 3);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter2, 300);

  gtk_text_buffer_apply_tag (buffer, tag, &iter, &iter2);  
  
  tag = gtk_text_buffer_create_tag (buffer, "end_tag", NULL);
  gtk_text_buffer_get_end_iter (buffer, &iter2);
  gtk_text_iter_backward_chars (&iter2, 12);
  iter = iter2;
  gtk_text_iter_backward_chars (&iter, 157);

  gtk_text_buffer_apply_tag (buffer, tag, &iter, &iter2);
  
  tag = gtk_text_buffer_create_tag (buffer, "center_tag", NULL);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter,
                                      gtk_text_buffer_get_char_count (buffer)/2);
  gtk_text_iter_backward_chars (&iter, 37);
  iter2 = iter;
  gtk_text_iter_forward_chars (&iter2, 57);

  gtk_text_buffer_apply_tag (buffer, tag, &iter, &iter2);  

  gdk_pixbuf_unref (pixbuf);
}
