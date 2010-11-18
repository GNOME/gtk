#include <gtk/gtk.h>
#include "cellareascaffold.h"

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
  GtkTreeIter   iter;
  GtkTreeStore *store = 
    gtk_tree_store_new (N_SIMPLE_COLUMNS,
			G_TYPE_STRING,  /* name text */
			G_TYPE_STRING,  /* icon name */
			G_TYPE_STRING); /* description text */

  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Alice in wonderland",
		      SIMPLE_COLUMN_ICON, "gtk-execute",
		      SIMPLE_COLUMN_DESCRIPTION, 
		      "Twas brillig, and the slithy toves "
		      "did gyre and gimble in the wabe",
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
		      SIMPLE_COLUMN_ICON, "gtk-dialog-warning",
		      SIMPLE_COLUMN_DESCRIPTION, "It's a very good question, very direct, "
		      "and I'm not going to answer it",
		      -1);

  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Whinnie the pooh",
		      SIMPLE_COLUMN_ICON, "gtk-stop",
		      SIMPLE_COLUMN_DESCRIPTION, "The most wonderful thing about tiggers, "
		      "is tiggers are wonderful things",
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

static GtkWidget *
simple_tree_menu (void)
{
  GtkTreeModel *model;
  GtkWidget *menu;
  GtkCellArea *area;
  GtkCellRenderer *renderer;

  model = simple_tree_model ();

  menu = gtk_tree_menu_new ();
  gtk_tree_menu_set_model (GTK_TREE_MENU (menu), model);

  area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (menu));

  cell_1 = renderer = gtk_cell_renderer_text_new ();
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "text", SIMPLE_COLUMN_NAME);

  cell_2 = renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (G_OBJECT (renderer), "xalign", 0.0F, NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, TRUE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "stock-id", SIMPLE_COLUMN_ICON);

  cell_3 = renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), 
		"wrap-mode", PANGO_WRAP_WORD,
		"wrap-width", 215,
		NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, TRUE);
  gtk_cell_area_attribute_connect (area, renderer, "text", SIMPLE_COLUMN_DESCRIPTION);

  return menu;
}

static void
align_cell_2_toggled (GtkToggleButton  *toggle,
		      GtkTreeMenu    *menu)
{
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (menu));
  gboolean     align = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_2, "align", align, NULL);
}

static void
align_cell_3_toggled (GtkToggleButton  *toggle,
		      GtkTreeMenu    *menu)
{
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (menu));
  gboolean     align = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_3, "align", align, NULL);
}

static void
expand_cell_1_toggled (GtkToggleButton  *toggle,
		       GtkTreeMenu      *menu)
{
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (menu));
  gboolean     expand = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_1, "expand", expand, NULL);
}

static void
expand_cell_2_toggled (GtkToggleButton  *toggle,
		       GtkTreeMenu      *menu)
{
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (menu));
  gboolean     expand = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_2, "expand", expand, NULL);
}

static void
expand_cell_3_toggled (GtkToggleButton  *toggle,
		       GtkTreeMenu      *menu)
{
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (menu));
  gboolean     expand = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_3, "expand", expand, NULL);
}

static void
tree_menu (void)
{
  GtkWidget *window, *widget;
  GtkWidget *menu, *menubar, *vbox, *menuitem;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title (GTK_WINDOW (window), "GtkTreeMenu");

  menu = simple_tree_menu ();

  vbox  = gtk_vbox_new (FALSE, 4);
  gtk_widget_show (vbox);

  menubar = gtk_menu_bar_new ();
  menuitem = gtk_menu_item_new_with_label ("Tree");
  gtk_widget_show (menu);
  gtk_widget_show (menubar);
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);

  gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, FALSE, 0);

  /* Now add some controls */
  widget = gtk_check_button_new_with_label ("Align 2nd Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (align_cell_2_toggled), menu);

  widget = gtk_check_button_new_with_label ("Align 3rd Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (align_cell_3_toggled), menu);

  widget = gtk_check_button_new_with_label ("Expand 1st Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (expand_cell_1_toggled), menu);

  widget = gtk_check_button_new_with_label ("Expand 2nd Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (expand_cell_2_toggled), menu);

  widget = gtk_check_button_new_with_label ("Expand 3rd Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (expand_cell_3_toggled), menu);

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
