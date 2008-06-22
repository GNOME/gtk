
#define GTK_ENABLE_BROKEN
#include "config.h"
#include <gtk/gtk.h>

/* for all the GtkItem:: and GtkTreeItem:: signals */
static void cb_itemsignal( GtkWidget *item,
                           gchar     *signame )
{
  gchar *name;
  GtkLabel *label;

  /* It's a Bin, so it has one child, which we know to be a
     label, so get that */
  label = GTK_LABEL (GTK_BIN (item)->child);
  /* Get the text of the label */
  gtk_label_get (label, &name);
  /* Get the level of the tree which the item is in */
  g_print ("%s called for item %s->%p, level %d\n", signame, name,
	   item, GTK_TREE (item->parent)->level);
}

/* Note that this is never called */
static void cb_unselect_child( GtkWidget *root_tree,
                               GtkWidget *child,
                               GtkWidget *subtree )
{
  g_print ("unselect_child called for root tree %p, subtree %p, child %p\n",
	   root_tree, subtree, child);
}

/* Note that this is called every time the user clicks on an item,
   whether it is already selected or not. */
static void cb_select_child (GtkWidget *root_tree, GtkWidget *child,
			     GtkWidget *subtree)
{
  g_print ("select_child called for root tree %p, subtree %p, child %p\n",
	   root_tree, subtree, child);
}

static void cb_selection_changed( GtkWidget *tree )
{
  GList *i;
  
  g_print ("selection_change called for tree %p\n", tree);
  g_print ("selected objects are:\n");

  i = GTK_TREE_SELECTION_OLD (tree);
  while (i) {
    gchar *name;
    GtkLabel *label;
    GtkWidget *item;

    /* Get a GtkWidget pointer from the list node */
    item = GTK_WIDGET (i->data);
    label = GTK_LABEL (GTK_BIN (item)->child);
    gtk_label_get (label, &name);
    g_print ("\t%s on level %d\n", name, GTK_TREE
	     (item->parent)->level);
    i = i->next;
  }
}

int main( int   argc,
          char *argv[] )
{
  GtkWidget *window, *scrolled_win, *tree;
  static gchar *itemnames[] = {"Foo", "Bar", "Baz", "Quux",
			       "Maurice"};
  gint i;

  gtk_init (&argc, &argv);

  /* a generic toplevel window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "delete_event",
                    G_CALLBACK (gtk_main_quit), NULL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 5);

  /* A generic scrolled window */
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_widget_set_size_request (scrolled_win, 150, 200);
  gtk_container_add (GTK_CONTAINER (window), scrolled_win);
  gtk_widget_show (scrolled_win);
  
  /* Create the root tree */
  tree = gtk_tree_new ();
  g_print ("root tree is %p\n", tree);
  /* connect all GtkTree:: signals */
  g_signal_connect (G_OBJECT (tree), "select_child",
                    G_CALLBACK (cb_select_child), tree);
  g_signal_connect (G_OBJECT (tree), "unselect_child",
                    G_CALLBACK (cb_unselect_child), tree);
  g_signal_connect (G_OBJECT(tree), "selection_changed",
                    G_CALLBACK(cb_selection_changed), tree);
  /* Add it to the scrolled window */
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_win),
                                         tree);
  /* Set the selection mode */
  gtk_tree_set_selection_mode (GTK_TREE (tree),
			       GTK_SELECTION_MULTIPLE);
  /* Show it */
  gtk_widget_show (tree);

  for (i = 0; i < 5; i++){
    GtkWidget *subtree, *item;
    gint j;

    /* Create a tree item */
    item = gtk_tree_item_new_with_label (itemnames[i]);
    /* Connect all GtkItem:: and GtkTreeItem:: signals */
    g_signal_connect (G_OBJECT (item), "select",
                      G_CALLBACK (cb_itemsignal), "select");
    g_signal_connect (G_OBJECT (item), "deselect",
                      G_CALLBACK (cb_itemsignal), "deselect");
    g_signal_connect (G_OBJECT (item), "toggle",
                      G_CALLBACK (cb_itemsignal), "toggle");
    g_signal_connect (G_OBJECT (item), "expand",
                      G_CALLBACK (cb_itemsignal), "expand");
    g_signal_connect (G_OBJECT (item), "collapse",
                      G_CALLBACK (cb_itemsignal), "collapse");
    /* Add it to the parent tree */
    gtk_tree_append (GTK_TREE (tree), item);
    /* Show it - this can be done at any time */
    gtk_widget_show (item);
    /* Create this item's subtree */
    subtree = gtk_tree_new ();
    g_print ("-> item %s->%p, subtree %p\n", itemnames[i], item,
	     subtree);

    /* This is still necessary if you want these signals to be called
       for the subtree's children.  Note that selection_change will be 
       signalled for the root tree regardless. */
    g_signal_connect (G_OBJECT (subtree), "select_child",
			G_CALLBACK (cb_select_child), subtree);
    g_signal_connect (G_OBJECT (subtree), "unselect_child",
			G_CALLBACK (cb_unselect_child), subtree);
    /* This has absolutely no effect, because it is completely ignored 
       in subtrees */
    gtk_tree_set_selection_mode (GTK_TREE (subtree),
				 GTK_SELECTION_SINGLE);
    /* Neither does this, but for a rather different reason - the
       view_mode and view_line values of a tree are propagated to
       subtrees when they are mapped.  So, setting it later on would
       actually have a (somewhat unpredictable) effect */
    gtk_tree_set_view_mode (GTK_TREE (subtree), GTK_TREE_VIEW_ITEM);
    /* Set this item's subtree - note that you cannot do this until
       AFTER the item has been added to its parent tree! */
    gtk_tree_item_set_subtree (GTK_TREE_ITEM (item), subtree);

    for (j = 0; j < 5; j++){
      GtkWidget *subitem;

      /* Create a subtree item, in much the same way */
      subitem = gtk_tree_item_new_with_label (itemnames[j]);
      /* Connect all GtkItem:: and GtkTreeItem:: signals */
      g_signal_connect (G_OBJECT (subitem), "select",
			  G_CALLBACK (cb_itemsignal), "select");
      g_signal_connect (G_OBJECT (subitem), "deselect",
			  G_CALLBACK (cb_itemsignal), "deselect");
      g_signal_connect (G_OBJECT (subitem), "toggle",
			  G_CALLBACK (cb_itemsignal), "toggle");
      g_signal_connect (G_OBJECT (subitem), "expand",
			  G_CALLBACK (cb_itemsignal), "expand");
      g_signal_connect (G_OBJECT (subitem), "collapse",
			  G_CALLBACK (cb_itemsignal), "collapse");
      g_print ("-> -> item %s->%p\n", itemnames[j], subitem);
      /* Add it to its parent tree */
      gtk_tree_append (GTK_TREE (subtree), subitem);
      /* Show it */
      gtk_widget_show (subitem);
    }
  }

  /* Show the window and loop endlessly */
  gtk_widget_show (window);

  gtk_main();

  return 0;
}
