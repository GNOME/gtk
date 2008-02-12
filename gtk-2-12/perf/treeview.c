#include <gtk/gtk.h>
#include "widgets.h"

struct row_data {
  char *stock_id;
  char *text1;
  char *text2;
};

static const struct row_data row_data[] = {
  { GTK_STOCK_NEW,		"First",		"Here bygynneth the Book of the tales of Caunterbury." },
  { GTK_STOCK_OPEN,		"Second",		"Whan that Aprille, with hise shoures soote," },
  { GTK_STOCK_ABOUT, 		"Third",		"The droghte of March hath perced to the roote" },
  { GTK_STOCK_ADD, 		"Fourth",		"And bathed every veyne in swich licour," },
  { GTK_STOCK_APPLY, 		"Fifth",		"Of which vertu engendred is the flour;" },
  { GTK_STOCK_BOLD, 		"Sixth",		"Whan Zephirus eek with his swete breeth" },
  { GTK_STOCK_CANCEL,		"Seventh",		"Inspired hath in every holt and heeth" },
  { GTK_STOCK_CDROM,		"Eighth",		"The tendre croppes, and the yonge sonne" },
  { GTK_STOCK_CLEAR,		"Ninth",		"Hath in the Ram his halfe cours yronne," },
  { GTK_STOCK_CLOSE,		"Tenth",		"And smale foweles maken melodye," },
  { GTK_STOCK_COLOR_PICKER,	"Eleventh",		"That slepen al the nyght with open eye-" },
  { GTK_STOCK_CONVERT,		"Twelfth",		"So priketh hem Nature in hir corages-" },
  { GTK_STOCK_CONNECT,		"Thirteenth",		"Thanne longen folk to goon on pilgrimages" },
  { GTK_STOCK_COPY,		"Fourteenth",		"And palmeres for to seken straunge strondes" },
  { GTK_STOCK_CUT,		"Fifteenth",		"To ferne halwes, kowthe in sondry londes;" },
  { GTK_STOCK_DELETE,		"Sixteenth",		"And specially, from every shires ende" },
  { GTK_STOCK_DIRECTORY,	"Seventeenth",		"Of Engelond, to Caunturbury they wende," },
  { GTK_STOCK_DISCONNECT,	"Eighteenth",		"The hooly blisful martir for the seke" },
  { GTK_STOCK_EDIT,		"Nineteenth",		"That hem hath holpen, whan that they were seeke." },
  { GTK_STOCK_EXECUTE,		"Twentieth",		"Bifil that in that seson, on a day," },
  { GTK_STOCK_FILE,		"Twenty-first",		"In Southwerk at the Tabard as I lay," },
  { GTK_STOCK_FIND,		"Twenty-second",	"Redy to wenden on my pilgrymage" },
  { GTK_STOCK_FIND_AND_REPLACE,	"Twenty-third",		"To Caunterbury, with ful devout corage," },
  { GTK_STOCK_FLOPPY,		"Twenty-fourth",	"At nyght were come into that hostelrye" },
  { GTK_STOCK_FULLSCREEN,	"Twenty-fifth",		"Wel nyne and twenty in a compaignye" },
  { GTK_STOCK_GOTO_BOTTOM,	"Twenty-sixth",		"Of sondry folk, by aventure yfalle" },
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
						     "stock-id", 0,
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
