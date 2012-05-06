#include <gtk/gtk.h>


/*******************************************************
 *                       Grid Test                     *
 *******************************************************/

#if _GTK_TREE_MENU_WAS_A_PUBLIC_CLASS_
static GdkPixbuf *
create_color_pixbuf (const char *color)
{
  GdkPixbuf *pixbuf;
  GdkRGBA rgba;

  int x;
  int num;
  int rowstride;
  guchar *pixels, *p;
  
  if (!gdk_rgba_parse (color, &col))
    return NULL;
  
  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
			   FALSE, 8,
			   16, 16);
  
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  p = pixels = gdk_pixbuf_get_pixels (pixbuf);
  
  num = gdk_pixbuf_get_width (pixbuf) *
    gdk_pixbuf_get_height (pixbuf);
  
  for (x = 0; x < num; x++) {
    p[0] = col.red * 255;
    p[1] = col.green * 255;
    p[2] = col.blue * 255;
    p += 3;
  }
  
  return pixbuf;
}

static GtkWidget *
create_menu_grid_demo (void)
{
  GtkWidget *menu;
  GtkTreeIter iter;
  GdkPixbuf *pixbuf;
  GtkCellRenderer *cell = gtk_cell_renderer_pixbuf_new ();
  GtkListStore *store;
  
  store = gtk_list_store_new (1, GDK_TYPE_PIXBUF);

  menu = gtk_tree_menu_new_full (NULL, GTK_TREE_MODEL (store), NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (menu), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (menu), cell, "pixbuf", 0, NULL);
  
  gtk_tree_menu_set_wrap_width (GTK_TREE_MENU (menu), 3);

  /* first row */
  pixbuf = create_color_pixbuf ("red");
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      -1);
  g_object_unref (pixbuf);
  
  pixbuf = create_color_pixbuf ("green");
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      -1);
  g_object_unref (pixbuf);
  
  pixbuf = create_color_pixbuf ("blue");
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      -1);
  g_object_unref (pixbuf);
  
  /* second row */
  pixbuf = create_color_pixbuf ("yellow");
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      -1);
  g_object_unref (pixbuf);
  
  pixbuf = create_color_pixbuf ("black");
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      -1);
  g_object_unref (pixbuf);
  
  pixbuf = create_color_pixbuf ("white");
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      -1);
  g_object_unref (pixbuf);
  
  /* third row */
  pixbuf = create_color_pixbuf ("gray");
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      -1);
  g_object_unref (pixbuf);
  
  pixbuf = create_color_pixbuf ("snow");
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      -1);
  g_object_unref (pixbuf);
  
  pixbuf = create_color_pixbuf ("magenta");
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      -1);
  g_object_unref (pixbuf);

  g_object_unref (store);
  
  return menu;
}
#endif

/*******************************************************
 *                      Simple Test                    *
 *******************************************************/
enum {
  SIMPLE_COLUMN_NAME,
  SIMPLE_COLUMN_ICON,
  SIMPLE_COLUMN_DESCRIPTION,
  N_SIMPLE_COLUMNS
};

static GtkCellRenderer *cell_1 = NULL, *cell_2 = NULL, *cell_3 = NULL;

static GtkTreeModel *
simple_tree_model (void)
{
  GtkTreeIter   iter, parent, child;
  GtkTreeStore *store = 
    gtk_tree_store_new (N_SIMPLE_COLUMNS,
			G_TYPE_STRING,  /* name text */
			G_TYPE_STRING,  /* icon name */
			G_TYPE_STRING); /* description text */


  gtk_tree_store_append (store, &parent, NULL);
  gtk_tree_store_set (store, &parent, 
		      SIMPLE_COLUMN_NAME, "Alice in wonderland",
		      SIMPLE_COLUMN_ICON, "gtk-execute",
		      SIMPLE_COLUMN_DESCRIPTION, 
		      "Twas brillig, and the slithy toves "
		      "did gyre and gimble in the wabe",
		      -1);

  gtk_tree_store_append (store, &iter, &parent);
  gtk_tree_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Go ask",
		      SIMPLE_COLUMN_ICON, "gtk-zoom-out",
		      SIMPLE_COLUMN_DESCRIPTION, "One pill makes you shorter",
		      -1);

  gtk_tree_store_append (store, &iter, &parent);
  gtk_tree_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Alice",
		      SIMPLE_COLUMN_ICON, "gtk-zoom-in",
		      SIMPLE_COLUMN_DESCRIPTION, "Another one makes you tall",
		      -1);

  gtk_tree_store_append (store, &iter, &parent);
  gtk_tree_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Jefferson Airplane",
		      SIMPLE_COLUMN_ICON, "gtk-zoom-fit",
		      SIMPLE_COLUMN_DESCRIPTION, "The one's that mother gives you dont do anything at all",
		      -1);

  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Marry Poppins",
		      SIMPLE_COLUMN_ICON, "gtk-yes",
		      SIMPLE_COLUMN_DESCRIPTION, "Supercalifragilisticexpialidocious",
		      -1);

  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "George Bush",
		      SIMPLE_COLUMN_ICON, "gtk-dialog-question",
		      SIMPLE_COLUMN_DESCRIPTION, "It's a very good question, very direct, "
		      "and I'm not going to answer it",
		      -1);

  gtk_tree_store_append (store, &parent, NULL);
  gtk_tree_store_set (store, &parent, 
		      SIMPLE_COLUMN_NAME, "Whinnie the pooh",
		      SIMPLE_COLUMN_ICON, "gtk-stop",
		      SIMPLE_COLUMN_DESCRIPTION, "The most wonderful thing about tiggers, "
		      "is tiggers are wonderful things",
		      -1);

  gtk_tree_store_append (store, &iter, &parent);
  gtk_tree_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Tigger",
		      SIMPLE_COLUMN_ICON, "gtk-yes",
		      SIMPLE_COLUMN_DESCRIPTION, "Eager",
		      -1);

  gtk_tree_store_append (store, &child, &iter);
  gtk_tree_store_set (store, &child, 
		      SIMPLE_COLUMN_NAME, "Jump",
		      SIMPLE_COLUMN_ICON, "gtk-yes",
		      SIMPLE_COLUMN_DESCRIPTION, "Very High",
		      -1);

  gtk_tree_store_append (store, &child, &iter);
  gtk_tree_store_set (store, &child, 
		      SIMPLE_COLUMN_NAME, "Pounce",
		      SIMPLE_COLUMN_ICON, "gtk-no",
		      SIMPLE_COLUMN_DESCRIPTION, "On Pooh",
		      -1);

  gtk_tree_store_append (store, &child, &iter);
  gtk_tree_store_set (store, &child, 
		      SIMPLE_COLUMN_NAME, "Bounce",
		      SIMPLE_COLUMN_ICON, "gtk-cancel",
		      SIMPLE_COLUMN_DESCRIPTION, "Around",
		      -1);

  gtk_tree_store_append (store, &iter, &parent);
  gtk_tree_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Owl",
		      SIMPLE_COLUMN_ICON, "gtk-stop",
		      SIMPLE_COLUMN_DESCRIPTION, "Wise",
		      -1);

  gtk_tree_store_append (store, &iter, &parent);
  gtk_tree_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Eor",
		      SIMPLE_COLUMN_ICON, "gtk-no",
		      SIMPLE_COLUMN_DESCRIPTION, "Depressed",
		      -1);

  gtk_tree_store_append (store, &iter, &parent);
  gtk_tree_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Piglet",
		      SIMPLE_COLUMN_ICON, "gtk-media-play",
		      SIMPLE_COLUMN_DESCRIPTION, "Insecure",
		      -1);

  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Aleister Crowley",
		      SIMPLE_COLUMN_ICON, "gtk-about",
		      SIMPLE_COLUMN_DESCRIPTION, 
		      "Thou shalt do what thou wilt shall be the whole of the law",
		      -1);

  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Mark Twain",
		      SIMPLE_COLUMN_ICON, "gtk-quit",
		      SIMPLE_COLUMN_DESCRIPTION, 
		      "Giving up smoking is the easiest thing in the world. "
		      "I know because I've done it thousands of times.",
		      -1);


  return (GtkTreeModel *)store;
}

static GtkCellArea *
create_cell_area (void)
{
  GtkCellArea *area;
  GtkCellRenderer *renderer;

  area = gtk_cell_area_box_new ();

  cell_1 = renderer = gtk_cell_renderer_text_new ();
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, FALSE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "text", SIMPLE_COLUMN_NAME);

  cell_2 = renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (G_OBJECT (renderer), "xalign", 0.0F, NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, TRUE, FALSE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "stock-id", SIMPLE_COLUMN_ICON);

  cell_3 = renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), 
		"wrap-mode", PANGO_WRAP_WORD,
		"wrap-width", 215,
		NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, TRUE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "text", SIMPLE_COLUMN_DESCRIPTION);

  return area;
}

#if _GTK_TREE_MENU_WAS_A_PUBLIC_CLASS_
static GtkWidget *
simple_tree_menu (GtkCellArea *area)
{
  GtkTreeModel *model;
  GtkWidget *menu;

  model = simple_tree_model ();

  menu = gtk_tree_menu_new_with_area (area);
  gtk_tree_menu_set_model (GTK_TREE_MENU (menu), model);

  return menu;
}
#endif

static void
orientation_changed (GtkComboBox  *combo,
		     GtkCellArea  *area)
{
  GtkOrientation orientation = gtk_combo_box_get_active (combo);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (area), orientation);
}

static void
align_cell_2_toggled (GtkToggleButton  *toggle,
		      GtkCellArea      *area)
{
  gboolean align = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_2, "align", align, NULL);
}

static void
align_cell_3_toggled (GtkToggleButton  *toggle,
		      GtkCellArea      *area)
{
  gboolean align = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_3, "align", align, NULL);
}

static void
expand_cell_1_toggled (GtkToggleButton  *toggle,
		       GtkCellArea      *area)
{
  gboolean expand = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_1, "expand", expand, NULL);
}

static void
expand_cell_2_toggled (GtkToggleButton  *toggle,
		       GtkCellArea      *area)
{
  gboolean expand = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_2, "expand", expand, NULL);
}

static void
expand_cell_3_toggled (GtkToggleButton  *toggle,
		       GtkCellArea      *area)
{
  gboolean expand = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_3, "expand", expand, NULL);
}

gboolean 
enable_submenu_headers (GtkTreeModel      *model,
			GtkTreeIter       *iter,
			gpointer           data)
{
  return TRUE;
}


#if _GTK_TREE_MENU_WAS_A_PUBLIC_CLASS_
static void
menu_activated_cb (GtkTreeMenu *menu,
		   const gchar *path,
		   gpointer     unused)
{
  GtkTreeModel *model = gtk_tree_menu_get_model (menu);
  GtkTreeIter   iter;
  gchar        *row_name;

  if (!gtk_tree_model_get_iter_from_string (model, &iter, path))
    return;

  gtk_tree_model_get (model, &iter, SIMPLE_COLUMN_NAME, &row_name, -1);

  g_print ("Item activated: %s\n", row_name);

  g_free (row_name);
}

static void
submenu_headers_toggled (GtkToggleButton  *toggle,
			 GtkTreeMenu      *menu)
{
  if (gtk_toggle_button_get_active (toggle))
    gtk_tree_menu_set_header_func (menu, enable_submenu_headers, NULL, NULL);
  else
    gtk_tree_menu_set_header_func (menu, NULL, NULL, NULL);
}

static void
tearoff_toggled (GtkToggleButton *toggle,
		 GtkTreeMenu     *menu)
{
  gtk_tree_menu_set_tearoff (menu, gtk_toggle_button_get_active (toggle));
}
#endif

static void
tree_menu (void)
{
  GtkWidget *window, *widget;
  GtkWidget *menubar, *vbox;
  GtkCellArea *area;
  GtkTreeModel *store;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title (GTK_WINDOW (window), "GtkTreeMenu");

  vbox  = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_show (vbox);

  menubar = gtk_menu_bar_new ();
  gtk_widget_show (menubar);

  store = simple_tree_model ();
  area  = create_cell_area ();

#if _GTK_TREE_MENU_WAS_A_PUBLIC_CLASS_
  menuitem = gtk_menu_item_new_with_label ("Grid");
  menu = create_menu_grid_demo ();
  gtk_widget_show (menu);
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);

  menuitem = gtk_menu_item_new_with_label ("Tree");
  menu = simple_tree_menu ();
  gtk_widget_show (menu);
  gtk_widget_show (menuitem);
  gtk_menu_shell_prepend (GTK_MENU_SHELL (menubar), menuitem);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);

  g_signal_connect (menu, "menu-activate", G_CALLBACK (menu_activated_cb), NULL);

  gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, FALSE, 0);
#endif

  /* Add a combo box with the same menu ! */
  widget = gtk_combo_box_new_with_area (area);
  gtk_combo_box_set_model (GTK_COMBO_BOX (widget), store);
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_show (widget);
  gtk_box_pack_end (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

  /* Now add some controls */
  widget = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Horizontal");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Vertical");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (orientation_changed), area);

  widget = gtk_check_button_new_with_label ("Align 2nd Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (align_cell_2_toggled), area);

  widget = gtk_check_button_new_with_label ("Align 3rd Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (align_cell_3_toggled), area);

  widget = gtk_check_button_new_with_label ("Expand 1st Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (expand_cell_1_toggled), area);

  widget = gtk_check_button_new_with_label ("Expand 2nd Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (expand_cell_2_toggled), area);

  widget = gtk_check_button_new_with_label ("Expand 3rd Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (expand_cell_3_toggled), area);

#if _GTK_TREE_MENU_WAS_A_PUBLIC_CLASS_
  widget = gtk_check_button_new_with_label ("Submenu Headers");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (submenu_headers_toggled), menu);

  widget = gtk_check_button_new_with_label ("Tearoff menu");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (tearoff_toggled), menu);
#endif

  gtk_container_add (GTK_CONTAINER (window), vbox);

  gtk_widget_show (window);
}

int
main (int argc, char *argv[])
{
  gtk_init (NULL, NULL);

  tree_menu ();

  gtk_main ();

  return 0;
}
