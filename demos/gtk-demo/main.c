#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <gtk/gtk.h>

#include <demos.h>

static GtkTextBuffer *info_buffer;
static GtkTextBuffer *source_buffer;
static GtkWidget *demo_clist;

static Demo *current_demo;

gboolean
read_line (FILE *stream, GString *str)
{
  int n_read = 0;
  
  flockfile (stream);

  g_string_truncate (str, 0);
  
  while (1)
    {
      int c;
      
      c = getc_unlocked (stream);

      if (c == EOF)
	goto done;
      else
	n_read++;

      switch (c)
	{
	case '\r':
	case '\n':
	  {
	    int next_c = getc_unlocked (stream);
	    
	    if (!(next_c == EOF ||
		  (c == '\r' && next_c == '\n') ||
		  (c == '\n' && next_c == '\r')))
	      ungetc (next_c, stream);
	    
	    goto done;
	  }
	default:
	  g_string_append_c (str, c);
	}
    }

 done:

  funlockfile (stream);

  return n_read > 0;
}

void
load_file (Demo *demo)
{
  FILE *file;
  GtkTextIter start, end;
  GString *buffer = g_string_new (NULL);
  int state = 0;
  gboolean in_para = 0;

  if (current_demo == demo)
    return;

  current_demo = demo;
  
  gtk_text_buffer_get_bounds (info_buffer, &start, &end);
  gtk_text_buffer_delete (info_buffer, &start, &end);

  gtk_text_buffer_get_bounds (source_buffer, &start, &end);
  gtk_text_buffer_delete (source_buffer, &start, &end);

  file = fopen (demo->filename, "r");
  if (!file)
    {
      g_warning ("Cannot open %s: %s\n", demo->filename, g_strerror (errno));
      return;
    }

  gtk_text_buffer_get_iter_at_offset (info_buffer, &start, 0);
  while (read_line (file, buffer))
    {
      gchar *p = buffer->str;
      gchar *q;
      
      switch (state)
	{
	case 0:
	  /* Reading title */
	  while (*p == '/' || *p == '*' || isspace (*p))
	    p++;
	  q = p + strlen (p);
	  while (q > p && isspace (*(q - 1)))
	    q--;

	  if (q > p)
	    {
	      int len_chars = g_utf8_pointer_to_offset (p, q);

	      end = start;

	      g_assert (strlen (p) >= q - p);
	      gtk_text_buffer_insert (info_buffer, &end, p, q - p);
	      start = end;

	      gtk_text_iter_backward_chars (&start, len_chars);
	      gtk_text_buffer_apply_tag_by_name (info_buffer, "title", &start, &end);

	      start = end;
	      
	      state++;
	    }
	  break;
	    
	case 1:
	  /* Reading body of info section */
	  while (isspace (*p))
	    p++;
	  if (*p == '*' && *(p + 1) == '/')
	    {
	      gtk_text_buffer_get_iter_at_offset (source_buffer, &start, 0);
	      state++;
	    }
	  else
	    {
	      int len;
	      
	      while (*p == '*' || isspace (*p))
		p++;

	      len = strlen (p);
	      while (isspace (*(p + len - 1)))
		len--;
	      
	      if (len > 0)
		{
		  if (in_para)
		    gtk_text_buffer_insert (info_buffer, &start, " ", 1);

		  g_assert (strlen (p) >= len);
		  gtk_text_buffer_insert (info_buffer, &start, p, len);
		  in_para = 1;
		}
	      else
		{
		  gtk_text_buffer_insert (info_buffer, &start, "\n", 1);
		  in_para = 0;
		}
	    }
	  break;

	case 2:
	  /* Skipping blank lines */
	  while (isspace (*p))
	    p++;
	  if (*p)
	    {
	      p = buffer->str;
	      state++;
	      /* Fall through */
	    }
	  else
	    break;
	  
	case 3:
	  /* Reading program body */
	  gtk_text_buffer_insert (source_buffer, &start, p, -1);
	  gtk_text_buffer_insert (info_buffer, &start, "\n", 1);
	  break;
	}
    }

  gtk_text_buffer_get_bounds (source_buffer, &start, &end);
  gtk_text_buffer_apply_tag_by_name (info_buffer, "source", &start, &end);
}

gboolean
button_press_event_cb (GtkCList       *clist,
		       GdkEventButton *event)
{
  if (event->type == GDK_2BUTTON_PRESS &&
      event->window == clist->clist_window)
    {
      gint row, column;
      
      if (gtk_clist_get_selection_info (clist,
					event->x, event->y,
					&row, &column))

	{
	  Demo *demo = gtk_clist_get_row_data (clist, row);
	  demo->func();
	}

      gtk_signal_emit_stop_by_name (GTK_OBJECT (clist), "button_press_event");
      return TRUE;
    }
  
  return FALSE;
}

void
select_row_cb (GtkCList *clist,
	       gint      row,
	       gint      column,
	       GdkEvent *event)
{
  Demo *demo = gtk_clist_get_row_data (clist, row);
  load_file (demo);
}

GtkWidget *
create_text (GtkTextBuffer **buffer,
	     gboolean        is_source)
{
  GtkWidget *scrolled_window;
  GtkWidget *text_view;
  PangoFontDescription *font_desc;

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
				       GTK_SHADOW_IN);
  
  text_view = gtk_text_view_new ();
  gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);
  
  *buffer = gtk_text_buffer_new (NULL);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (text_view), *buffer);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);

  if (is_source)
    {
      font_desc = pango_font_description_from_string ("Courier 10");
      gtk_widget_modify_font (text_view, font_desc);
      pango_font_description_free (font_desc);
    }
  
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view), !is_source);
  
  return scrolled_window;
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *notebook;
  GtkWidget *hbox;
  GtkTextTag *tag;
  int i;

  gtk_init (&argc, &argv);
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), hbox);

  demo_clist = gtk_clist_new (1);
  gtk_clist_set_selection_mode (GTK_CLIST (demo_clist), GTK_SELECTION_BROWSE);

  gtk_widget_set_usize (demo_clist, 200, -1);
  gtk_box_pack_start (GTK_BOX (hbox), demo_clist, FALSE, FALSE, 0);

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (hbox), notebook, TRUE, TRUE, 0);

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    create_text (&info_buffer, FALSE),
			    gtk_label_new ("Info"));


  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    create_text (&source_buffer, TRUE),
			    gtk_label_new ("Source"));

  tag = gtk_text_buffer_create_tag (info_buffer, "title");
  gtk_object_set (GTK_OBJECT (tag),
		 "font", "Sans 18",
		 NULL);

  tag = gtk_text_buffer_create_tag (info_buffer, "source");
  gtk_object_set (GTK_OBJECT (tag),
		  "font", "Courier 10",
		  "pixels_above_lines", 0,
		  "pixels_below_lines", 0,
		 NULL);

  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
  gtk_widget_show_all (window);
  
  for (i=0; i < G_N_ELEMENTS (testgtk_demos); i++)
    {
      gint row = gtk_clist_append (GTK_CLIST (demo_clist), &testgtk_demos[i].title);
      gtk_clist_set_row_data (GTK_CLIST (demo_clist), row, &testgtk_demos[i]);
    }

  load_file (&testgtk_demos[0]);
  
  gtk_signal_connect (GTK_OBJECT (demo_clist), "button_press_event",
		      GTK_SIGNAL_FUNC (button_press_event_cb), NULL);
  
  gtk_signal_connect (GTK_OBJECT (demo_clist), "select_row",
		      GTK_SIGNAL_FUNC (select_row_cb), NULL);
  
  gtk_main ();

  return 0;
}
