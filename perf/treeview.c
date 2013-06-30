#include <gtk/gtk.h>
#include "widgets.h"

struct row_data {
  char *stock_id;
  char *text1;
  char *text2;
};

static const struct row_data row_data[] = {
  { "document-new",		"First",		"Here bygynneth the Book of the tales of Caunterbury." },
  { "document-open",		"Second",		"Whan that Aprille, with hise shoures soote," },
  { "help-about", 		"Third",		"The droghte of March hath perced to the roote" },
  { "list-add", 		"Fourth",		"And bathed every veyne in swich licour," },
  { "go-top", 		"Fifth",		"Of which vertu engendred is the flour;" },
  { "format-text-bold", 		"Sixth",		"Whan Zephirus eek with his swete breeth" },
  { "go-first",		"Seventh",		"Inspired hath in every holt and heeth" },
  { "media-optical",		"Eighth",		"The tendre croppes, and the yonge sonne" },
  { "edit-clear",		"Ninth",		"Hath in the Ram his halfe cours yronne," },
  { "window-close",		"Tenth",		"And smale foweles maken melodye," },
  { "go-last",	"Eleventh",		"That slepen al the nyght with open eye-" },
  { "go-previous",		"Twelfth",		"So priketh hem Nature in hir corages-" },
  { "go-down",		"Thirteenth",		"Thanne longen folk to goon on pilgrimages" },
  { "edit-copy",		"Fourteenth",		"And palmeres for to seken straunge strondes" },
  { "edit-cut",		"Fifteenth",		"To ferne halwes, kowthe in sondry londes;" },
  { "edit-delete",		"Sixteenth",		"And specially, from every shires ende" },
  { "folder",	"Seventeenth",		"Of Engelond, to Caunturbury they wende," },
  { "go-next",	"Eighteenth",		"The hooly blisful martir for the seke" },
  { "go-up",		"Nineteenth",		"That hem hath holpen, whan that they were seeke." },
  { "system-run",		"Twentieth",		"Bifil that in that seson, on a day," },
  { "text-x-generic",		"Twenty-first",		"In Southwerk at the Tabard as I lay," },
  { "edit-find",		"Twenty-second",	"Redy to wenden on my pilgrymage" },
  { "edit-find-replace",	"Twenty-third",		"To Caunterbury, with ful devout corage," },
  { "media-floppy",		"Twenty-fourth",	"At nyght were come into that hostelrye" },
  { "view-fullscreen",	"Twenty-fifth",		"Wel nyne and twenty in a compaignye" },
  { "go-bottom",	"Twenty-sixth",		"Of sondry folk, by aventure yfalle" },
};

static GtkTreeModel *
tree_model_new (void)
{
  GtkListStore *list;
  int i;

  list = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  for (i = 0; i < G_N_ELEMENTS (row_data); i++)
    {
      GtkTreeIter iter;

      gtk_list_store_append (list, &iter);
      gtk_list_store_set (list,
			  &iter,
			  0, row_data[i].stock_id,
			  1, row_data[i].text1,
			  2, row_data[i].text2,
			  -1);
    }

  return GTK_TREE_MODEL (list);
}

GtkWidget *
tree_view_new (void)
{
  GtkWidget *sw;
  GtkWidget *tree;
  GtkTreeModel *model;
  GtkTreeViewColumn *column;

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

  model = tree_model_new ();
  tree = gtk_tree_view_new_with_model (model);
  g_object_unref (model);

  gtk_widget_set_size_request (tree, 300, 100);

  column = gtk_tree_view_column_new_with_attributes ("Icon",
						     gtk_cell_renderer_pixbuf_new (),
						     "icon-name", 0,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  column = gtk_tree_view_column_new_with_attributes ("Index",
						     gtk_cell_renderer_text_new (),
						     "text", 1,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  column = gtk_tree_view_column_new_with_attributes ("Canterbury Tales",
						     gtk_cell_renderer_text_new (),
						     "text", 2,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  gtk_container_add (GTK_CONTAINER (sw), tree);

  return sw;
}
