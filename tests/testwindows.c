#undef GDK_DISABLE_DEPRECATED
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#endif

static GtkWidget *darea;
static GtkTreeStore *window_store = NULL;
static GtkWidget *treeview;

static void update_store (void);

static GtkWidget *main_window;

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

static void
add_window_cb (GtkTreeModel      *model,
	       GtkTreePath       *path,
	       GtkTreeIter       *iter,
	       gpointer           data)
{
  GList **selected = data;
  GdkWindow *window;

  gtk_tree_model_get (GTK_TREE_MODEL (window_store),
		      iter,
		      0, &window,
		      -1);

  *selected = g_list_prepend (*selected, window);
}

static GList *
get_selected_windows (void)
{
  GtkTreeSelection *sel;
  GList *selected;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  selected = NULL;
  gtk_tree_selection_selected_foreach (sel, add_window_cb, &selected);
  
  return selected;
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
toggle_selection_window (GdkWindow *window)
{
  GtkTreeSelection *selection;
  GtkTreeIter iter;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  if (window != NULL &&
      find_window (window, &iter))
    {
      if (gtk_tree_selection_iter_is_selected (selection, &iter))
	gtk_tree_selection_unselect_iter (selection,  &iter);
      else
	gtk_tree_selection_select_iter (selection,  &iter);
    }
}

static void
unselect_windows (void)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  
  gtk_tree_selection_unselect_all (selection);
}


static void
select_window (GdkWindow *window)
{
  GtkTreeSelection *selection;
  GtkTreeIter iter;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  if (window != NULL &&
      find_window (window, &iter))
    gtk_tree_selection_select_iter (selection,  &iter);
}

static void
select_windows (GList *windows)
{
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GList *l;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_unselect_all (selection);
  
  for (l = windows; l != NULL; l = l->next)
    {
      if (find_window (l->data, &iter))
	gtk_tree_selection_select_iter (selection,  &iter);
    }
}

static void
add_window_clicked (GtkWidget *button, 
		    gpointer data)
{
  GdkWindow *parent;
  GList *l;

  l = get_selected_windows ();
  if (l != NULL)
    parent = l->data;
  else
    parent = darea->window;

  g_list_free (l);
  
  create_window (parent, 10, 10, 100, 100, NULL);
  update_store ();
}

static void
draw_drawable_clicked (GtkWidget *button, 
		       gpointer data)
{
  GdkGC *gc;
  gc = gdk_gc_new (darea->window);
  gdk_draw_drawable (darea->window,
		     gc,
		     darea->window,
		     -15, -15,
		     40, 70,
		     100, 100);
  g_object_unref (gc);
}


static void
remove_window_clicked (GtkWidget *button, 
		       gpointer data)
{
  GList *l, *selected;

  selected = get_selected_windows ();

  for (l = selected; l != NULL; l = l->next)
    gdk_window_destroy (l->data);

  g_list_free (selected);

  update_store ();
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
	gdk_window_ensure_native (window);
      
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
  GList *selected, *l;
  gint x, y;

  direction = GPOINTER_TO_INT (data);
    
  selected = get_selected_windows ();

  for (l = selected; l != NULL; l = l->next)
    {
      window = l->data;
      
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

  g_list_free (selected);
}

static void
manual_clicked (GtkWidget *button, 
		gpointer data)
{
  GdkWindow *window;
  GList *selected, *l;
  int x, y, w, h;
  GtkWidget *dialog, *table, *label, *xspin, *yspin, *wspin, *hspin;
  

  selected = get_selected_windows ();

  if (selected == NULL)
    return;

  gdk_window_get_position (selected->data, &x, &y);
  gdk_drawable_get_size (selected->data, &w, &h);

  dialog = gtk_dialog_new_with_buttons ("Select new position and size",
					GTK_WINDOW (main_window),
					GTK_DIALOG_MODAL,
					GTK_STOCK_OK, GTK_RESPONSE_OK,
					NULL);
  

  table = gtk_table_new (2, 4, TRUE);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
		      table,
		      FALSE, FALSE,
		      2);

  
  label = gtk_label_new ("x:");
  gtk_table_attach_defaults (GTK_TABLE (table),
			     label,
			     0, 1,
			     0, 1);
  label = gtk_label_new ("y:");
  gtk_table_attach_defaults (GTK_TABLE (table),
			     label,
			     0, 1,
			     1, 2);
  label = gtk_label_new ("width:");
  gtk_table_attach_defaults (GTK_TABLE (table),
			     label,
			     0, 1,
			     2, 3);
  label = gtk_label_new ("height:");
  gtk_table_attach_defaults (GTK_TABLE (table),
			     label,
			     0, 1,
			     3, 4);

  xspin = gtk_spin_button_new_with_range (G_MININT, G_MAXINT, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (xspin), x);
  gtk_table_attach_defaults (GTK_TABLE (table),
			     xspin,
			     1, 2,
			     0, 1);
  yspin = gtk_spin_button_new_with_range (G_MININT, G_MAXINT, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (yspin), y);
  gtk_table_attach_defaults (GTK_TABLE (table),
			     yspin,
			     1, 2,
			     1, 2);
  wspin = gtk_spin_button_new_with_range (G_MININT, G_MAXINT, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (wspin), w);
  gtk_table_attach_defaults (GTK_TABLE (table),
			     wspin,
			     1, 2,
			     2, 3);
  hspin = gtk_spin_button_new_with_range (G_MININT, G_MAXINT, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (hspin), h);
  gtk_table_attach_defaults (GTK_TABLE (table),
			     hspin,
			     1, 2,
			     3, 4);
  
  gtk_widget_show_all (dialog);
  
  gtk_dialog_run (GTK_DIALOG (dialog));

  x = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (xspin));
  y = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (yspin));
  w = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (wspin));
  h = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (hspin));

  gtk_widget_destroy (dialog);
  
  for (l = selected; l != NULL; l = l->next)
    {
      window = l->data;
      
      gdk_window_move_resize (window, x, y, w, h);
    }

  g_list_free (selected);
}

static void
restack_clicked (GtkWidget *button,
		 gpointer data)
{
  GList *selected;

  selected = get_selected_windows ();

  if (g_list_length (selected) != 2)
    {
      g_warning ("select two windows");
    }

  gdk_window_restack (selected->data,
		      selected->next->data,
		      GPOINTER_TO_INT (data));

  g_list_free (selected);

  update_store ();
}

static void
scroll_window_clicked (GtkWidget *button, 
		       gpointer data)
{
  GdkWindow *window;
  GtkDirectionType direction;
  GList *selected, *l;
  gint dx, dy;

  direction = GPOINTER_TO_INT (data);
    
  selected = get_selected_windows ();

  dx = 0; dy = 0;
  switch (direction) {
  case GTK_DIR_UP:
    dy = 10;
    break;
  case GTK_DIR_DOWN:
    dy = -10;
    break;
  case GTK_DIR_LEFT:
    dx = 10;
    break;
  case GTK_DIR_RIGHT:
    dx = -10;
    break;
  default:
    break;
  }
  
  for (l = selected; l != NULL; l = l->next)
    {
      window = l->data;

      gdk_window_scroll (window, dx, dy);
    }

  g_list_free (selected);
}


static void
raise_window_clicked (GtkWidget *button, 
		      gpointer data)
{
  GList *selected, *l;
  GdkWindow *window;
    
  selected = get_selected_windows ();

  for (l = selected; l != NULL; l = l->next)
    {
      window = l->data;
      
      gdk_window_raise (window);
    }

  g_list_free (selected);
  
  update_store ();
}

static void
lower_window_clicked (GtkWidget *button, 
		      gpointer data)
{
  GList *selected, *l;
  GdkWindow *window;
    
  selected = get_selected_windows ();

  for (l = selected; l != NULL; l = l->next)
    {
      window = l->data;
      
      gdk_window_lower (window);
    }

  g_list_free (selected);
  
  update_store ();
}


static void
smaller_window_clicked (GtkWidget *button, 
			gpointer data)
{
  GList *selected, *l;
  GdkWindow *window;
  int w, h;

  selected = get_selected_windows ();

  for (l = selected; l != NULL; l = l->next)
    {
      window = l->data;
      
      gdk_drawable_get_size (GDK_DRAWABLE (window), &w, &h);
      
      w -= 10;
      h -= 10;
      if (w < 1)
	w = 1;
      if (h < 1)
	h = 1;
      
      gdk_window_resize (window, w, h);
    }

  g_list_free (selected);
}

static void
larger_window_clicked (GtkWidget *button, 
			gpointer data)
{
  GList *selected, *l;
  GdkWindow *window;
  int w, h;

  selected = get_selected_windows ();

  for (l = selected; l != NULL; l = l->next)
    {
      window = l->data;
      
      gdk_drawable_get_size (GDK_DRAWABLE (window), &w, &h);
      
      w += 10;
      h += 10;
      
      gdk_window_resize (window, w, h);
    }

  g_list_free (selected);
}

static void
native_window_clicked (GtkWidget *button, 
			gpointer data)
{
  GList *selected, *l;
  GdkWindow *window;

  selected = get_selected_windows ();

  for (l = selected; l != NULL; l = l->next)
    {
      window = l->data;
      
      gdk_window_ensure_native (window);
    }
  
  g_list_free (selected);
  
  update_store ();
}

static gboolean
darea_button_release_event (GtkWidget *widget,
			    GdkEventButton *event)
{
  if ((event->state & GDK_CONTROL_MASK) != 0)
    {
      toggle_selection_window (event->window);
    }
  else
    {
      unselect_windows ();
      select_window (event->window);
    }
    
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
  GList *selected;

  selected = get_selected_windows ();

  gtk_tree_store_clear (window_store);

  add_children (window_store, darea->window, NULL);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (treeview));

  select_windows (selected);
  g_list_free (selected);
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

  main_window = window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
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
			       GTK_SELECTION_MULTIPLE);
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
  
  table = gtk_table_new (5, 4, TRUE);
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


  button = gtk_button_new_with_label ("scroll");
  gtk_button_set_image (GTK_BUTTON (button),
			gtk_image_new_from_stock (GTK_STOCK_GO_UP,
						  GTK_ICON_SIZE_BUTTON));
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (scroll_window_clicked), 
		    GINT_TO_POINTER (GTK_DIR_UP));
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     3, 4,
			     0, 1);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("scroll");
  gtk_button_set_image (GTK_BUTTON (button),
			gtk_image_new_from_stock (GTK_STOCK_GO_DOWN,
						  GTK_ICON_SIZE_BUTTON));
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (scroll_window_clicked), 
		    GINT_TO_POINTER (GTK_DIR_DOWN));
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     3, 4,
			     1, 2);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("Manual");
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (manual_clicked),
		    NULL);
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     3, 4,
			     2, 3);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("Restack above");
  g_signal_connect (button, "clicked",
		    G_CALLBACK (restack_clicked),
		    GINT_TO_POINTER (1));
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     2, 3,
			     3, 4);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("Restack below");
  g_signal_connect (button, "clicked",
		    G_CALLBACK (restack_clicked),
		    0);
  gtk_table_attach_defaults (GTK_TABLE (table),
			     button,
			     3, 4,
			     3, 4);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("draw drawable");
  gtk_box_pack_start (GTK_BOX (vbox),
		      button,
		      FALSE, FALSE,
		      2);
  gtk_widget_show (button);
  g_signal_connect (button, "clicked", 
		    G_CALLBACK (draw_drawable_clicked), 
		    NULL);

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
