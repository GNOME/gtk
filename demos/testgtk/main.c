#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <gtk/gtk.h>

#include <demos.h>

static GtkTextBuffer *info_buffer;
static GtkTextBuffer *source_buffer;

static gchar *current_file = NULL;

enum {
  TITLE_COLUMN,
  FILENAME_COLUMN,
  FUNC_COLUMN,
  ITALIC_COLUMN,
  NUM_COLUMNS
};

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
load_file (const gchar *filename)
{
  FILE *file;
  GtkTextIter start, end;
  GString *buffer = g_string_new (NULL);
  int state = 0;
  gboolean in_para = 0;

  if (current_file && !strcmp (current_file, filename))
    return;

  g_free (current_file);
  current_file = g_strdup (filename);
  
  gtk_text_buffer_get_bounds (info_buffer, &start, &end);
  gtk_text_buffer_delete (info_buffer, &start, &end);

  gtk_text_buffer_get_bounds (source_buffer, &start, &end);
  gtk_text_buffer_delete (source_buffer, &start, &end);

  file = fopen (filename, "r");
  if (!file)
    {
      g_warning ("Cannot open %s: %s\n", filename, g_strerror (errno));
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
  fclose (file);
}

gboolean
button_press_event_cb (GtkTreeView    *tree_view,
		       GdkEventButton *event,
		       GtkTreeModel   *model)
{
  if (event->type == GDK_2BUTTON_PRESS)
    {
      GtkTreePath *path = NULL;

      gtk_tree_view_get_path_at_pos (tree_view,
				     event->window,
				     event->x,
				     event->y,
				     &path,
				     NULL);

      if (path)
	{
	  GtkTreeIter iter;
	  gboolean italic;
	  GVoidFunc func;

	  gtk_tree_model_get_iter (model, &iter, path);
	  gtk_tree_store_get (GTK_TREE_STORE (model),
			      &iter,
			      FUNC_COLUMN, &func,
			      ITALIC_COLUMN, &italic,
			      -1);
	  (func) ();
	  gtk_tree_store_set (GTK_TREE_STORE (model),
			      &iter,
			      ITALIC_COLUMN, !italic,
			      -1);
	  gtk_tree_path_free (path);
	}

      g_signal_stop_emission_by_name (tree_view, "button-press-event");
      return TRUE;
    }
  
  return FALSE;
}

static void
selection_cb (GtkTreeSelection *selection,
	      GtkTreeModel     *model)
{
  GtkTreeIter iter;
  GValue value = {0, };

  if (! gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  gtk_tree_model_get_value (model, &iter,
			    FILENAME_COLUMN,
			    &value);
  load_file (g_value_get_string (&value));
  g_value_unset (&value);
}

static GtkWidget *
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

/* Technically a list, but if we do go to 80 demos, we may want to move to a tree */
static GtkWidget *
create_tree (void)
{
  GtkTreeSelection *selection;
  GtkCellRenderer *cell;
  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkTreeStore *model;
  GtkTreeIter iter;
  gint i;

  model = gtk_tree_store_new_with_types (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_BOOLEAN);
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

  gtk_tree_selection_set_type (GTK_TREE_SELECTION (selection),
			       GTK_TREE_SELECTION_SINGLE);
  gtk_widget_set_usize (tree_view, 200, -1);

  for (i=0; i < G_N_ELEMENTS (testgtk_demos); i++)
    {
      gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

      gtk_tree_store_set (GTK_TREE_STORE (model),
			  &iter,
			  TITLE_COLUMN, testgtk_demos[i].title,
			  FILENAME_COLUMN, testgtk_demos[i].filename,
			  FUNC_COLUMN, testgtk_demos[i].func,
			  ITALIC_COLUMN, FALSE,
			  -1);
    }

  cell = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Widget",
						     cell,
						     "text", TITLE_COLUMN,
						     "italic", ITALIC_COLUMN,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
			       GTK_TREE_VIEW_COLUMN (column));

  g_signal_connect (selection, "selection-changed", selection_cb, model);
  g_signal_connect (tree_view, "button-press-event", G_CALLBACK (button_press_event_cb), model);

  return tree_view;
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *notebook;
  GtkWidget *hbox;
  GtkWidget *tree;
  GtkTextTag *tag;

  gtk_init (&argc, &argv);
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy",
                    G_CALLBACK (gtk_main_quit), NULL);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), hbox);

  tree = create_tree ();
  gtk_box_pack_start (GTK_BOX (hbox), tree, FALSE, FALSE, 0);

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
  

  load_file (testgtk_demos[0].filename);
  
  gtk_main ();

  return 0;
}
