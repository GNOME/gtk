/* testtreesort.c
 * Copyright (C) 2001 Red Hat, Inc
 * Author: Jonathan Blandford
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <gtk/gtk.h>


typedef struct _ListSort ListSort;
struct _ListSort
{
  const gchar *word_1;
  const gchar *word_2;
  const gchar *word_3;
  const gchar *word_4;
  gint number_1;
};

static ListSort data[] =
{
  { "Apples", "Transmogrify long word to demonstrate weirdness", "Exculpatory", "Gesundheit", 30 },
  { "Oranges", "Wicker", "Adamantine", "Convivial", 10 },
  { "Bovine Spongiform Encephilopathy", "Sleazebucket", "Mountaineer", "Pander", 40 },
  { "Foot and Mouth", "Lampshade", "Skim Milk\nFull Milk", "Viewless", 20 },
  { "Blood,\nsweat,\ntears", "The Man", "Horses", "Muckety-Muck", 435 },
  { "Rare Steak", "Siam", "Watchdog", "Xantippe" , 99999 },
  { "SIGINT", "Rabbit Breath", "Alligator", "Bloodstained", 4123 },
  { "Google", "Chrysanthemums", "Hobnob", "Leapfrog", 1 },
  { "Technology fibre optic", "Turtle", "Academe", "Lonely", 3 },
  { "Freon", "Harpes", "Quidditch", "Reagan", 6},
  { "Transposition", "Fruit Basket", "Monkey Wort", "Glogg", 54 },
  { "Fern", "Glasnost and Perestroika", "Latitude", "Bomberman!!!", 2 },
  {NULL, }
};

static ListSort childdata[] =
{
  { "Heineken", "Nederland", "Wanda de vis", "Electronische post", 2},
  { "Hottentottententententoonstelling", "Rotterdam", "Ionentransport", "Palm", 45},
  { "Fruitvlieg", "Eigenfrequentie", "Supernoodles", "Ramen", 2002},
  { "Gereedschapskist", "Stelsel van lineaire vergelijkingen", "Tulpen", "Badlaken", 1311},
  { "Stereoinstallatie", "Rood tapijt", "Het periodieke systeem der elementen", "Laaste woord", 200},
  {NULL, }
};
  

enum
{
  WORD_COLUMN_1 = 0,
  WORD_COLUMN_2,
  WORD_COLUMN_3,
  WORD_COLUMN_4,
  NUMBER_COLUMN_1,
  NUM_COLUMNS
};

gboolean
select_func (GtkTreeSelection  *selection,
	     GtkTreeModel      *model,
	     GtkTreePath       *path,
	     gboolean           path_currently_selected,
	     gpointer           data)
{
  if (gtk_tree_path_get_depth (path) > 1)
    return TRUE;
  return FALSE;
}

static void
switch_search_method (GtkWidget *button,
		      gpointer   tree_view)
{
  if (!gtk_tree_view_get_search_entry (GTK_TREE_VIEW (tree_view)))
    {
      gpointer data = g_object_get_data (tree_view, "my-search-entry");
      gtk_tree_view_set_search_entry (GTK_TREE_VIEW (tree_view), GTK_ENTRY (data));
    }
  else
    gtk_tree_view_set_search_entry (GTK_TREE_VIEW (tree_view), NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *scrolled_window;
  GtkWidget *tree_view;
  GtkTreeStore *model;
  GtkTreeModel *smodel = NULL;
  GtkTreeModel *ssmodel = NULL;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeIter iter;
  gint i;

  GtkWidget *entry, *button;
  GtkWidget *window2, *vbox2, *scrolled_window2, *tree_view2;
  GtkWidget *window3, *vbox3, *scrolled_window3, *tree_view3;

  gtk_init (&argc, &argv);

  /**
   * First window - Just a GtkTreeStore
   */

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Words, words, words - Window 1");
  g_signal_connect (window, "destroy", gtk_main_quit, NULL);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_box_pack_start (GTK_BOX (vbox), gtk_label_new ("Jonathan and Kristian's list of cool words. (And Anders' cool list of numbers) \n\nThis is just a GtkTreeStore"), FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Switch search method");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);

  model = gtk_tree_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

/*
  smodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (model));
  ssmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (smodel));
*/
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));

  gtk_tree_view_set_search_entry (GTK_TREE_VIEW (tree_view), GTK_ENTRY (entry));
  g_object_set_data (G_OBJECT (tree_view), "my-search-entry", entry);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (switch_search_method), tree_view);

 /* gtk_tree_selection_set_select_function (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)), select_func, NULL, NULL);*/

  /* 12 iters now, 12 later... */
  for (i = 0; data[i].word_1 != NULL; i++)
    {
      gint k;
      GtkTreeIter child_iter;


      gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter, NULL);
      gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			  WORD_COLUMN_1, data[i].word_1,
			  WORD_COLUMN_2, data[i].word_2,
			  WORD_COLUMN_3, data[i].word_3,
			  WORD_COLUMN_4, data[i].word_4,
			  NUMBER_COLUMN_1, data[i].number_1,
			  -1);

      gtk_tree_store_append (GTK_TREE_STORE (model), &child_iter, &iter);
      gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
			  WORD_COLUMN_1, data[i].word_1,
			  WORD_COLUMN_2, data[i].word_2,
			  WORD_COLUMN_3, data[i].word_3,
			  WORD_COLUMN_4, data[i].word_4,
			  NUMBER_COLUMN_1, data[i].number_1,
			  -1);

      for (k = 0; childdata[k].word_1 != NULL; k++)
	{
	  gtk_tree_store_append (GTK_TREE_STORE (model), &child_iter, &iter);
	  gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
			      WORD_COLUMN_1, childdata[k].word_1,
			      WORD_COLUMN_2, childdata[k].word_2,
			      WORD_COLUMN_3, childdata[k].word_3,
			      WORD_COLUMN_4, childdata[k].word_4,
			      NUMBER_COLUMN_1, childdata[k].number_1,
			      -1);

	}

    }
  
  smodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (model));
  ssmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (smodel));
  g_object_unref (model);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("First Word", renderer,
						     "text", WORD_COLUMN_1,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
  gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_1);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Second Word", renderer,
						     "text", WORD_COLUMN_2,
						     NULL);
  gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_2);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Third Word", renderer,
						     "text", WORD_COLUMN_3,
						     NULL);
  gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_3);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Fourth Word", renderer,
						     "text", WORD_COLUMN_4,
						     NULL);
  gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_4);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
  
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("First Number", renderer,
						     "text", NUMBER_COLUMN_1,
						     NULL);
  gtk_tree_view_column_set_sort_column_id (column, NUMBER_COLUMN_1);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  /*  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (smodel),
					WORD_COLUMN_1,
					GTK_SORT_ASCENDING);*/

  gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  gtk_widget_show_all (window);

  /**
   * Second window - GtkTreeModelSort wrapping the GtkTreeStore
   */

  if (smodel)
    {
      window2 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window2), 
			    "Words, words, words - window 2");
      g_signal_connect (window2, "destroy", gtk_main_quit, NULL);
      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
      gtk_box_pack_start (GTK_BOX (vbox2), 
			  gtk_label_new ("Jonathan and Kristian's list of words.\n\nA GtkTreeModelSort wrapping the GtkTreeStore of window 1"),
			  FALSE, FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window2), vbox2);
      
      scrolled_window2 = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window2),
					   GTK_SHADOW_ETCHED_IN);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window2),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (vbox2), scrolled_window2, TRUE, TRUE, 0);
      
      
      tree_view2 = gtk_tree_view_new_with_model (smodel);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("First Word", renderer,
							 "text", WORD_COLUMN_1,
							 NULL);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view2), column);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_1);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("Second Word", renderer,
							 "text", WORD_COLUMN_2,
							 NULL);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_2);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view2), column);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("Third Word", renderer,
							 "text", WORD_COLUMN_3,
							 NULL);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_3);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view2), column);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("Fourth Word", renderer,
							 "text", WORD_COLUMN_4,
							 NULL);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_4);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view2), column);
      
      /*      gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (smodel),
					       (GtkTreeIterCompareFunc)gtk_tree_data_list_compare_func,
					       NULL, NULL);
      gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (smodel),
					    WORD_COLUMN_1,
					    GTK_SORT_DESCENDING);*/
      
      
      gtk_container_add (GTK_CONTAINER (scrolled_window2), tree_view2);
      gtk_window_set_default_size (GTK_WINDOW (window2), 400, 400);
      gtk_widget_show_all (window2);
    }
  
  /**
   * Third window - GtkTreeModelSort wrapping the GtkTreeModelSort which
   * is wrapping the GtkTreeStore.
   */
  
  if (ssmodel)
    {
      window3 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window3), 
			    "Words, words, words - Window 3");
      g_signal_connect (window3, "destroy", gtk_main_quit, NULL);
      vbox3 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_container_set_border_width (GTK_CONTAINER (vbox3), 8);
      gtk_box_pack_start (GTK_BOX (vbox3), 
			  gtk_label_new ("Jonathan and Kristian's list of words.\n\nA GtkTreeModelSort wrapping the GtkTreeModelSort of window 2"),
			  FALSE, FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window3), vbox3);
      
      scrolled_window3 = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window3),
					   GTK_SHADOW_ETCHED_IN);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window3),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (vbox3), scrolled_window3, TRUE, TRUE, 0);
      
      
      tree_view3 = gtk_tree_view_new_with_model (ssmodel);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("First Word", renderer,
							 "text", WORD_COLUMN_1,
							 NULL);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view3), column);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_1);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("Second Word", renderer,
							 "text", WORD_COLUMN_2,
							 NULL);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_2);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view3), column);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("Third Word", renderer,
							 "text", WORD_COLUMN_3,
							 NULL);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_3);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view3), column);
      
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes ("Fourth Word", renderer,
							 "text", WORD_COLUMN_4,
							 NULL);
      gtk_tree_view_column_set_sort_column_id (column, WORD_COLUMN_4);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view3), column);
      
      /*      gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (ssmodel),
					       (GtkTreeIterCompareFunc)gtk_tree_data_list_compare_func,
					       NULL, NULL);
      gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (ssmodel),
					    WORD_COLUMN_1,
					    GTK_SORT_ASCENDING);*/
      
      gtk_container_add (GTK_CONTAINER (scrolled_window3), tree_view3);
      gtk_window_set_default_size (GTK_WINDOW (window3), 400, 400);
      gtk_widget_show_all (window3);
    }

  for (i = 0; data[i].word_1 != NULL; i++)
    {
      gint k;
      
      gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter, NULL);
      gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			  WORD_COLUMN_1, data[i].word_1,
			  WORD_COLUMN_2, data[i].word_2,
			  WORD_COLUMN_3, data[i].word_3,
			  WORD_COLUMN_4, data[i].word_4,
			  -1);
      for (k = 0; childdata[k].word_1 != NULL; k++)
	{
	  GtkTreeIter child_iter;
	  
	  gtk_tree_store_append (GTK_TREE_STORE (model), &child_iter, &iter);
	  gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
			      WORD_COLUMN_1, childdata[k].word_1,
			      WORD_COLUMN_2, childdata[k].word_2,
			      WORD_COLUMN_3, childdata[k].word_3,
			      WORD_COLUMN_4, childdata[k].word_4,
			      -1);
	}
    }

  gtk_main ();
  
  return 0;
}
