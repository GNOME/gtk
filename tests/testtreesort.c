#include <gtk/gtk.h>


typedef struct _ListSort ListSort;
struct _ListSort
{
  const gchar *word_1;
  const gchar *word_2;
  const gchar *word_3;
  const gchar *word_4;
};

static ListSort data[] =
{
  { "Apples", "Transmorgrify", "Exculpatory", "Gesundheit"},
  { "Oranges", "Wicker", "Adamantine", "Convivial" },
  { "Bovine Spongiform Encephilopathy", "Sleazebucket", "Mountaineer", "Pander" },
  { "Foot and Mouth", "Lampshade", "Skim Milk\nFull Milk", "Viewless" },
  { "Blood,\nsweat,\ntears", "The Man", "Horses", "Muckety-Muck" },
  { "Rare Steak", "Siam", "Watchdog", "Xantippe" },
  { "SIGINT", "Rabbit Breath", "Alligator", "Bloodstained" },
  { "Google", "Chrysanthemums", "Hobnob", "Leapfrog"},
  { "Technology fibre optic", "Turtle", "Academe", "Lonely"  },
  { "Freon", "Harpes", "Quidditch", "Reagan" },
  { "Transposition", "Fruit Basket", "Monkey Wort", "Glogg" },
  { "Fern", "Glasnost and Perestroika", "Latitude", "Bomberman!!!" },
  {NULL, }
};
  

enum
{
  WORD_COLUMN = 0,
  WORD_COLUMN_2,
  WORD_COLUMN_3,
  WORD_COLUMN_4,
  NUM_COLUMNS
};

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *scrolled_window;
  GtkWidget *tree_view;
  GtkTreeModel *model;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeIter iter;
  gint i;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect (GTK_OBJECT (window), "destroy", gtk_main_quit, NULL);
  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_box_pack_start (GTK_BOX (vbox), gtk_label_new ("My List of cool words"), FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);

  model = gtk_list_store_new_with_types (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
					0, GTK_TREE_SORT_ASCENDING);
  for (i = 0; data[i].word_1 != NULL; i++)
    {
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			  WORD_COLUMN, data[i].word_1,
			  WORD_COLUMN_2, data[i].word_2,
			  WORD_COLUMN_3, data[i].word_3,
			  WORD_COLUMN_4, data[i].word_4,
			  -1);
    }

  tree_view = gtk_tree_view_new_with_model (model);
  g_object_unref (G_OBJECT (model));
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree_view), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("First Word", renderer,
						     "text", WORD_COLUMN,
						     NULL);
  gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
  g_object_unref (column);
  g_object_unref (renderer);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Second Word", renderer,
						     "text", WORD_COLUMN_2,
						     NULL);
  gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_2);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
  g_object_unref (column);
  g_object_unref (renderer);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Third Word", renderer,
						     "text", WORD_COLUMN_3,
						     NULL);
  gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_3);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
  g_object_unref (column);
  g_object_unref (renderer);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Fourth Word", renderer,
						     "text", WORD_COLUMN_4,
						     NULL);
  gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_4);

  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
  g_object_unref (column);
  g_object_unref (renderer);

  gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
  gtk_widget_show_all (window);
  gtk_main ();

  return 0;
}
