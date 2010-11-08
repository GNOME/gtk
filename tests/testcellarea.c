#include <gtk/gtk.h>
#include "cellareascaffold.h"

enum {
  SIMPLE_COLUMN_NAME,
  SIMPLE_COLUMN_ICON,
  SIMPLE_COLUMN_DESCRIPTION,
  N_SIMPLE_COLUMNS
};

static GtkTreeModel *
simple_list_model (void)
{
  GtkTreeIter   iter;
  GtkListStore *store = 
    gtk_list_store_new (N_SIMPLE_COLUMNS,
			G_TYPE_STRING,  /* name text */
			G_TYPE_STRING,  /* icon name */
			G_TYPE_STRING); /* description text */

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Alice in wonderland",
		      SIMPLE_COLUMN_ICON, "gtk-execute",
		      SIMPLE_COLUMN_DESCRIPTION, "One pill makes you smaller and the other pill makes you tall",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Highschool Principal",
		      SIMPLE_COLUMN_ICON, "gtk-help",
		      SIMPLE_COLUMN_DESCRIPTION, 
		      "Will make you copy the dictionary if you dont like your math teacher",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Marry Poppins",
		      SIMPLE_COLUMN_ICON, "gtk-yes",
		      SIMPLE_COLUMN_DESCRIPTION, "Supercalifragilisticexpialidocious",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "George Bush",
		      SIMPLE_COLUMN_ICON, "gtk-dialog-warning",
		      SIMPLE_COLUMN_DESCRIPTION, "Please hide your nuclear weapons when inviting "
		      "him to dinner",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Whinnie the pooh",
		      SIMPLE_COLUMN_ICON, "gtk-stop",
		      SIMPLE_COLUMN_DESCRIPTION, "The most wonderful thing about tiggers, "
		      "is tiggers are wonderful things",
		      -1);

  return (GtkTreeModel *)store;
}

static void
simple_cell_area (void)
{
  GtkWidget *window;
  GtkTreeModel *model;
  GtkWidget *scaffold, *frame, *label, *box;
  GtkCellArea *area;
  GtkCellRenderer *renderer;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  scaffold = cell_area_scaffold_new ();
  gtk_widget_show (scaffold);

  model = simple_list_model ();

  cell_area_scaffold_set_model (CELL_AREA_SCAFFOLD (scaffold), model);

  area = cell_area_scaffold_get_area (CELL_AREA_SCAFFOLD (scaffold));

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "text", SIMPLE_COLUMN_NAME);

  renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (G_OBJECT (renderer), "xalign", 0.0F, NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, TRUE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "stock-id", SIMPLE_COLUMN_ICON);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), 
		"wrap-mode", PANGO_WRAP_WORD,
		"wrap-width", 215,
		NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, TRUE);
  gtk_cell_area_attribute_connect (area, renderer, "text", SIMPLE_COLUMN_DESCRIPTION);

  box   = gtk_vbox_new (FALSE, 4);
  frame = gtk_frame_new (NULL);
  label = gtk_label_new ("GtkCellArea below");
  gtk_widget_show (box);
  gtk_widget_show (frame);
  gtk_widget_show (label);

  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (frame), scaffold);
  gtk_container_add (GTK_CONTAINER (window), box);

  gtk_widget_show (window);
}

int
main (int argc, char *argv[])
{
  gtk_init (NULL, NULL);

  simple_cell_area ();

  gtk_main ();

  return 0;
}
