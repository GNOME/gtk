#include <gtk/gtk.h>
#include <X11/Xlib.h>

static GtkWidget *darea;
static GtkTreeStore *window_store = NULL;
static GtkWidget *treeview;

static void update_store (void);

static gboolean
window_has_impl (GdkWindow *window)
{
  GdkWindowObject *w;
  w = (GdkWindowObject *)window;
  return w->parent == NULL || w->parent->impl != w->impl;
}

GdkWindow *
create_window (GdkWindow *parent,
	       int x, int y, int w, int h,
	       GdkColor *color)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkWindow *window;
  GdkColor *bg;

  attributes.x = x;
  attributes.y = y;
  attributes.width = w;
  attributes.height = h;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = GDK_STRUCTURE_MASK
			| GDK_BUTTON_MOTION_MASK
			| GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK
			| GDK_EXPOSURE_MASK
			| GDK_ENTER_NOTIFY_MASK
			| GDK_LEAVE_NOTIFY_MASK;
  attributes.wclass = GDK_INPUT_OUTPUT;
      
  attributes_mask = GDK_WA_X | GDK_WA_Y;
      
  window = gdk_window_new (parent, &attributes, attributes_mask);
  gdk_window_set_user_data (window, darea);

  bg = g_new (GdkColor, 1);
  if (color)
    *bg = *color;
  else
    {
      bg->red = g_random_int_range (0, 0xffff);
      bg->blue = g_random_int_range (0, 0xffff);
      bg->green = g_random_int_range (0, 0xffff);;
    }
  
  gdk_rgb_find_color (gtk_widget_get_colormap (darea), bg);
  gdk_window_set_background (window, bg);
  g_object_set_data_full (G_OBJECT (window), "color", bg, g_free);
  
  gdk_window_show (window);
  
  return window;
}

static GdkWindow *
get_selected_window (void)
{
  GtkTreeSelection *sel;
  GtkTreeIter iter;
  GdkWindow *window;
  
  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  if (!gtk_tree_selection_get_selected (sel, NULL, &iter))
    return NULL;

  gtk_tree_model_get (GTK_TREE_MODEL (window_store),
		      &iter,
		      0, &window,
		      -1);
  return window;
}

static gboolean
find_window_helper (GtkTreeModel *model,
		    GdkWindow *window,
		    GtkTreeIter *iter,
		    GtkTreeIter *selected_iter)
{
  GtkTreeIter child_iter;
  GdkWindow *w;

  do
    {
      gtk_tree_model_get (model, iter,
			  0, &w,
			  -1);
      if (w == window)
	{
	  *selected_iter = *iter;
	  return TRUE;
	}
      
      if (gtk_tree_model_iter_children (model,
					&child_iter,
					iter))
	{
	  if (find_window_helper (model, window, &child_iter, selected_iter))
	    return TRUE;
	}
    } while (gtk_tree_model_iter_next (model, iter));

  return FALSE;
}

static gboolean
find_window (GdkWindow *window,
	     GtkTreeIter *window_iter)
{
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter_first  (GTK_TREE_MODEL (window_store), &iter))
    return FALSE;

  return find_window_helper (GTK_TREE_MODEL (window_store),
			     window,
			     &iter,
			     window_iter);
}

static void
select_window (GdkWindow *window)
{
  GtkTreeIter iter;
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  if (window != NULL &&
      find_window (window, &iter))
    gtk_tree_selection_select_iter (selection,  &iter);
  else
    gtk_tree_selection_unselect_all (selection);
}

static void
add_window_clicked (GtkWidget *button, 
		    gpointer data)
{
  GdkWindow *parent;

  parent = get_selected_window ();
  if (parent == NULL)
    parent = darea->window;
  
  create_window (parent, 10, 10, 100, 100, NULL);
  update_store ();
}

static void
remove_window_clicked (GtkWidget *button, 
		       gpointer data)
{
  GdkWindow *window;

  window = get_selected_window ();
  if (window == NULL)
    return;

  gdk_window_destroy (window);
}

static void save_children (GString *s, GdkWindow *window);

static void
save_window (GString *s,
	     GdkWindow *window)
{
  gint x, y, w, h;
  GdkColor *color;

  gdk_window_get_position (window, &x, &y);
  gdk_drawable_get_size (GDK_DRAWABLE (window), &w, &h);
  color = g_object_get_data (G_OBJECT (window), "color");
  
  g_string_append_printf (s, "%d,%d %dx%d (%d,%d,%d) %d %d\n",
			  x, y, w, h,
			  color->red, color->green, color->blue,
			  window_has_impl (window),
			  g_list_length (gdk_window_peek_children (window)));

  save_children (s, window);
}


static void
save_children (GString *s,
	       GdkWindow *window)
{
  GList *l;
  GdkWindow *child;

  for (l = g_list_reverse (gdk_window_peek_children (window));
       l != NULL;
       l = l->next)
    {
      child = l->data;

      save_window (s, child);
    }
}


static void
save_clicked (GtkWidget *button, 
	      gpointer data)
{
  GString *s;
  GtkWidget *dialog;
  GFile *file;

  s = g_string_new ("");

  save_children (s, darea->window);

  dialog = gtk_file_chooser_dialog_new ("Filename for window data",
					NULL,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					NULL);
  
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

      g_file_replace_contents (file,
			       s->str, s->len,
			       NULL, FALSE,
			       0, NULL, NULL, NULL);

      g_object_unref (file);
    }

  gtk_widget_destroy (dialog);
  g_string_free (s, TRUE);
}

static void
destroy_children (GdkWindow *window)
{
  GList *l;
  GdkWindow *child;

  for (l = gdk_window_peek_children (window);
       l != NULL;
       l = l->next)
    {
      child = l->data;
      
      destroy_children (child);
      gdk_window_destroy (child);
    }
}

static char **
parse_window (GdkWindow *parent, char **lines)
{
  int x, y, w, h, r, g, b, native, n_children;
  GdkWindow *window;
  GdkColor color;
  int i;

  if (*lines == NULL)
    return lines;
  
  if (sscanf(*lines, "%d,%d %dx%d (%d,%d,%d) %d %d",
	     &x, &y, &w, &h, &r, &g, &b, &native, &n_children) == 9)
    {
      lines++;
      color.red = r;
      color.green = g;
      color.blue = b;
      window = create_window (parent, x, y, w, h, &color);
      if (native)
	gdk_window_set_has_native (window, TRUE);
      
      for (i = 0; i < n_children; i++)
	lines = parse_window (window, lines);
    }
  else
    lines++;
  
  return lines;
}
  
static void
load_file (GFile *file)
{
  char *data;
  char **lines, **l;
  
  if (g_file_load_contents (file, NULL, &data, NULL, NULL, NULL))
    {
      destroy_children (darea->window);

      lines = g_strsplit (data, "\n", -1);

      l = lines;
      while (*l != NULL)
	l = parse_window (darea->window, l);
    }

  update_store ();
}

static void
move_window_clicked (GtkWidget *button, 
		     gpointer data)
{
  GdkWindow *window;
  GtkDirectionType direction;
  gint x, y;

  direction = GPOINTER_TO_INT (data);
    
  window = get_selected_window ();
  
  if (window == NULL)
    return;

  gdk_window_get_position (window, &x, &y);

  switch (direction) {
  case GTK_DIR_UP:
    y -= 10;
    break;
  case GTK_DIR_DOWN:
    y += 10;
    break;
  case GTK_DIR_LEFT:
    x -= 10;
    break;
  case GTK_DIR_RIGHT:
    x += 10;
    break;
  default:
    break;
  }

  gdk_window_move (window, x, y);
}

static void
raise_window_clicked (GtkWidget *button, 
		      gpointer data)
{
  GdkWindow *window;
    
  window = get_selected_window ();
  if (window == NULL)
    return;

  gdk_window_raise (window);
  update_store ();
}

static void
lower_window_clicked (GtkWidget *button, 
		      gpointer data)
{
  GdkWindow *window;
    
  window = get_selected_window ();
  if (window == NULL)
    return;

  gdk_window_lower (window);
  update_store ();
}


static void
smaller_window_clicked (GtkWidget *button, 
			gpointer data)
{
  GdkWindow *window;
  int w, h;

  window = get_selected_window ();
  if (window == NULL)
    return;

  gdk_drawable_get_size (GDK_DRAWABLE (window), &w, &h);

  w -= 10;
  h -= 10;
  if (w < 1)
    w = 1;
  if (h < 1)
    h = 1;

  gdk_window_resize (window, w, h);
}

static void
larger_window_clicked (GtkWidget *button, 
			gpointer data)
{
  GdkWindow *window;
  int w, h;

  window = get_selected_window ();
  if (window == NULL)
    return;

  gdk_drawable_get_size (GDK_DRAWABLE (window), &w, &h);

  w += 10;
  h += 10;

  gdk_window_resize (window, w, h);
}

static void
native_window_clicked (GtkWidget *button, 
			gpointer data)
{
  GdkWindow *window;

  window = get_selected_window ();
  if (window == NULL)
    return;

  gdk_window_set_has_native (window, TRUE);
  update_store ();
}

static gboolean
darea_button_release_event (GtkWidget *widget,
			    GdkEventButton *event)
{
  select_window (event->window);
  return TRUE;
}

static void
render_window_cell (GtkTreeViewColumn *tree_column,
		    GtkCellRenderer   *cell,
		    GtkTreeModel      *tree_model,
		    GtkTreeIter       *iter,
		    gpointer           data)
{
  GdkWindow *window;
  char *name;

  gtk_tree_model_get (GTK_TREE_MODEL (window_store),
		      iter,
		      0, &window,
		      -1);

  if (window_has_impl (window))
      name = g_strdup_printf ("%p (native)", window);
  else
      name = g_strdup_printf ("%p", window);
  g_object_set (cell,
		"text", name,
		"background-gdk", &((GdkWindowObject *)window)->bg_color,
		NULL);  
}

static void
add_children (GtkTreeStore *store,
	      GdkWindow *window,
	      GtkTreeIter *window_iter)
{
  GList *l;
  GtkTreeIter child_iter;

  for (l = gdk_window_peek_children (window);
       l != NULL;
       l = l->next)
    {
      gtk_tree_store_append (store, &child_iter, window_iter);
      gtk_tree_store_set (store, &child_iter,
			  0, l->data,
			  -1);

      add_children (store, l->data, &child_iter);
    }
}

static void
update_store (void)
{
  GdkWindow *selected;

  selected = get_selected_window ();

  gtk_tree_store_clear (window_store);

  add_children (window_store, darea->window, NULL);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (treeview));

  select_window (selected);
}


int
main (int argc, char **argv)
{
  GtkWidget *window, *vbox, *hbox, *frame;
  GtkWidget *button, *scrolled, *table;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GdkColor black = {0};
  GFile *file;
  
  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);

  g_signal_connect (G_OBJECT (window), "delete-event", gtk_main_quit, NULL);

  hbox = gtk_hbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (window), hbox);
  gtk_widget_show (hbox);

  frame = gtk_frame_new ("GdkWindows");
  gtk_box_pack_start (GTK_BOX (hbox),
		      frame,
		      FALSE, FALSE,
		      5);
  gtk_widget_show (frame);

  darea =  gtk_drawing_area_new ();
  /*gtk_widget_set_double_buffered (darea, FALSE);*/
  gtk_widget_add_events (darea, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
  gtk_widget_set_size_request (darea, 500, 500);
  g_signal_connect (darea, "button_release_event", 
		    G_CALLBACK (darea_button_release_event), 
		    NULL);

  
  gtk_container_add (GTK_CONTAINER (frame), darea);
  gtk_widget_realize (darea);
  gtk_widget_show (darea);
  gtk_widget_modify_bg (darea, GTK_STATE_NORMAL,
			&black);
			
  
  vbox = gtk_vbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (hbox),
		      vbox,
		      FALSE, FALSE,
		      5);
  gtk_widget_show (vbox);

  window_store = gtk_tree_store_new (1, GDK_TYPE_WINDOW);
  
  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (window_store));
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
			       GTK_SELECTION_SINGLE);
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, "Window");
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (column,
					   renderer,
					   render_window_cell,
					   NULL, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);


  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (scrolled, 200, 400);
  gtk_container_add (GTK_CONTAINER (scrolled), treeview);
  gtk_box_pack_start (GTK_BOX (vbox),
		      scrolled,
		      FALSE, FALSE,
		      5);
  gtk_widget_show (scrolled);
  gtk_widget_show (treeview);
  
  table = gtk_table_new (3, 3, TRUE);
  gtk_box_pack_start (GTK_BOX (vbox),
		      table,
		      FALSE, FALSE,
		      2);
  gtk_widget_show (table);

  button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (button),
			gtk_image_new_from_stock (GTK_STOCK_GO_BACK,
						  GTK_ICON_SIZE_BUTTON));
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (move_window_clicked), 
		    GINT_TO_POINTER (GTK_DIR_LEFT));
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     0, 1,
			     1, 2);
  gtk_widget_show (button);

  button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (button),
			gtk_image_new_from_stock (GTK_STOCK_GO_UP,
						  GTK_ICON_SIZE_BUTTON));
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (move_window_clicked), 
		    GINT_TO_POINTER (GTK_DIR_UP));
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     1, 2,
			     0, 1);
  gtk_widget_show (button);

  button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (button),
			gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD,
						  GTK_ICON_SIZE_BUTTON));
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (move_window_clicked), 
		    GINT_TO_POINTER (GTK_DIR_RIGHT));
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     2, 3,
			     1, 2);
  gtk_widget_show (button);

  button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (button),
			gtk_image_new_from_stock (GTK_STOCK_GO_DOWN,
						  GTK_ICON_SIZE_BUTTON));
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (move_window_clicked), 
		    GINT_TO_POINTER (GTK_DIR_DOWN));
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     1, 2,
			     2, 3);
  gtk_widget_show (button);


  button = gtk_button_new_with_label ("Raise");
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (raise_window_clicked), 
		    NULL);
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     0, 1,
			     0, 1);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("Lower");
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (lower_window_clicked), 
		    NULL);
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     0, 1,
			     2, 3);
  gtk_widget_show (button);


  button = gtk_button_new_with_label ("Smaller");
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (smaller_window_clicked), 
		    NULL);
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     2, 3,
			     0, 1);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("Larger");
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (larger_window_clicked), 
		    NULL);
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     2, 3,
			     2, 3);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("Native");
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (native_window_clicked), 
		    NULL);
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     1, 2,
			     1, 2);
  gtk_widget_show (button);


  button = gtk_button_new_with_label ("Add window");
  gtk_box_pack_start (GTK_BOX (vbox),
		      button,
		      FALSE, FALSE,
		      2);
  gtk_widget_show (button);
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (add_window_clicked), 
		    NULL);
  
  button = gtk_button_new_with_label ("Remove window");
  gtk_box_pack_start (GTK_BOX (vbox),
		      button,
		      FALSE, FALSE,
		      2);
  gtk_widget_show (button);
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (remove_window_clicked), 
		    NULL);

  button = gtk_button_new_with_label ("Save");
  gtk_box_pack_start (GTK_BOX (vbox),
		      button,
		      FALSE, FALSE,
		      2);
  gtk_widget_show (button);
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (save_clicked), 
		    NULL);

  gtk_widget_show (window);

  if (argc == 2)
    {
      file = g_file_new_for_commandline_arg (argv[1]);
      load_file (file);
      g_object_unref (file);
    }
  
  gtk_main ();

  return 0;
}
