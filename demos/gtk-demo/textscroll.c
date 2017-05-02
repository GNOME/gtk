/* Text Widget/Automatic scrolling
 *
 * This example demonstrates how to use the gravity of 
 * GtkTextMarks to keep a text view scrolled to the bottom
 * while appending text.
 */

#include <gtk/gtk.h>
#include "demo-common.h"

/* Scroll to the end of the buffer.
 */
static gboolean
scroll_to_end (GtkTextView *textview)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextMark *mark;
  char *spaces;
  static int count;

  buffer = gtk_text_view_get_buffer (textview);
  
  /* Get "end" mark. It's located at the end of buffer because 
   * of right gravity
   */
  mark = gtk_text_buffer_get_mark (buffer, "end");
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

  /* and insert some text at its position, the iter will be 
   * revalidated after insertion to point to the end of inserted text
   */
  spaces = g_strnfill (count++, ' ');
  gtk_text_buffer_insert (buffer, &iter, "\n", -1);
  gtk_text_buffer_insert (buffer, &iter, spaces, -1);
  gtk_text_buffer_insert (buffer, &iter,
                          "Scroll to end scroll to end scroll "
                          "to end scroll to end ",
                          -1);
  g_free (spaces);

  /* Now scroll the end mark onscreen.
   */
  gtk_text_view_scroll_mark_onscreen (textview, mark);

  /* Emulate typewriter behavior, shift to the left if we 
   * are far enough to the right.
   */
  if (count > 150)
    count = 0;

  return TRUE;
}

/* Scroll to the bottom of the buffer.
 */
static gboolean
scroll_to_bottom (GtkTextView *textview)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextMark *mark;
  char *spaces;
  static int count;

  buffer = gtk_text_view_get_buffer (textview);
  
  /* Get end iterator */
  gtk_text_buffer_get_end_iter (buffer, &iter);

  /* and insert some text at it, the iter will be revalidated
   * after insertion to point to the end of inserted text
   */
  spaces = g_strnfill (count++, ' ');
  gtk_text_buffer_insert (buffer, &iter, "\n", -1);
  gtk_text_buffer_insert (buffer, &iter, spaces, -1);
  gtk_text_buffer_insert (buffer, &iter,
                          "Scroll to bottom scroll to bottom scroll "
                          "to bottom scroll to bottom",
                          -1);
  g_free (spaces);

  /* Move the iterator to the beginning of line, so we don't scroll 
   * in horizontal direction 
   */
  gtk_text_iter_set_line_offset (&iter, 0);
  
  /* and place the mark at iter. the mark will stay there after we
   * insert some text at the end because it has right gravity.
   */
  mark = gtk_text_buffer_get_mark (buffer, "scroll");
  gtk_text_buffer_move_mark (buffer, mark, &iter);
  
  /* Scroll the mark onscreen.
   */
  gtk_text_view_scroll_mark_onscreen (textview, mark);

  /* Shift text back if we got enough to the right.
   */
  if (count > 40)
    count = 0;

  return TRUE;
}

static guint
setup_scroll (GtkTextView *textview,
              gboolean     to_end)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  buffer = gtk_text_view_get_buffer (textview);
  gtk_text_buffer_get_end_iter (buffer, &iter);

  if (to_end)
  {
    /* If we want to scroll to the end, including horizontal scrolling,
     * then we just create a mark with right gravity at the end of the 
     * buffer. It will stay at the end unless explicitely moved with 
     * gtk_text_buffer_move_mark.
     */
    gtk_text_buffer_create_mark (buffer, "end", &iter, FALSE);
    
    /* Add scrolling timeout. */
    return g_timeout_add (50, (GSourceFunc) scroll_to_end, textview);
  }
  else
  {
    /* If we want to scroll to the bottom, but not scroll horizontally, 
     * then an end mark won't do the job. Just create a mark so we can 
     * use it with gtk_text_view_scroll_mark_onscreen, we'll position it
     * explicitely when needed. Use left gravity so the mark stays where 
     * we put it after inserting new text.
     */
    gtk_text_buffer_create_mark (buffer, "scroll", &iter, TRUE);
    
    /* Add scrolling timeout. */
    return g_timeout_add (100, (GSourceFunc) scroll_to_bottom, textview);
  }
}

static void
remove_timeout (GtkWidget *window,
                gpointer   timeout)
{
  g_source_remove (GPOINTER_TO_UINT (timeout));
}

static void
create_text_view (GtkWidget *hbox,
                  gboolean   to_end)
{
  GtkWidget *swindow;
  GtkWidget *textview;
  guint timeout;

  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), swindow, TRUE, TRUE, 0);
  textview = gtk_text_view_new ();
  gtk_container_add (GTK_CONTAINER (swindow), textview);

  timeout = setup_scroll (GTK_TEXT_VIEW (textview), to_end);

  /* Remove the timeout in destroy handler, so we don't try to
   * scroll destroyed widget. 
   */
  g_signal_connect (textview, "destroy",
                    G_CALLBACK (remove_timeout),
                    GUINT_TO_POINTER (timeout));
}

GtkWidget *
do_textscroll (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *hbox;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &window);
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      
      hbox = gtk_hbox_new (TRUE, 6);
      gtk_container_add (GTK_CONTAINER (window), hbox);

      create_text_view (hbox, TRUE);
      create_text_view (hbox, FALSE);
    }

  if (!gtk_widget_get_visible (window))
      gtk_widget_show_all (window);
  else
      gtk_widget_destroy (window);

  return window;
}
