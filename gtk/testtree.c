#include "gtk.h"

typedef struct sTreeButtons {
    GtkWidget *button_add, *button_remove;
} sTreeButton;

static gint cb_delete_event() {
    return TRUE;
}
static void cb_destroy_event() {
    gtk_main_quit();
}
static void cb_tree_changed(GtkTree* tree) {
    sTreeButton* tree_buttons;
    GList* selected;
    gint nb_selected;

    tree_buttons = gtk_object_get_user_data(GTK_OBJECT(tree));

    selected = tree->selection;
    nb_selected = g_list_length(selected);

    if(nb_selected == 0) {
	if(tree->children == NULL)
	    gtk_widget_set_sensitive(tree_buttons->button_add, TRUE);
	else
	    gtk_widget_set_sensitive(tree_buttons->button_add, FALSE);
	gtk_widget_set_sensitive(tree_buttons->button_remove, FALSE);
    } else {
	gtk_widget_set_sensitive(tree_buttons->button_remove, TRUE);
	gtk_widget_set_sensitive(tree_buttons->button_add, (nb_selected == 1));
    }
}

static void add_tree_item(GtkWidget* w, GtkTree* tree) {
    static gint nb_item_add = 0;
    GList* selected;
    gint nb_selected;
    GtkTreeItem *selected_item;
    GtkWidget* new_item;
    GtkWidget* subtree;
    gchar buffer[255];

    selected = GTK_TREE_SELECTION(tree);
    nb_selected = g_list_length(selected);

    if(nb_selected > 1) return;

    if(nb_selected == 0 && tree->children != NULL) return;

    if(tree->children == NULL) {
	subtree = GTK_WIDGET(tree);
    } else {
	selected_item = GTK_TREE_ITEM(selected->data);
	subtree = GTK_TREE_ITEM_SUBTREE(selected_item);
    }
    if(!subtree) { /* create a new subtree if not exist */
	subtree = gtk_tree_new();
	gtk_signal_connect(GTK_OBJECT(subtree), "selection_changed",
			   (GtkSignalFunc)cb_tree_changed,
			   (gpointer)NULL);
	gtk_tree_item_set_subtree(GTK_TREE_ITEM(selected_item), subtree);
    }

    /* create a new item */
    sprintf(buffer, "new item %d", nb_item_add++);
    new_item = gtk_tree_item_new_with_label(buffer);
    gtk_tree_append(GTK_TREE(subtree), new_item);
    gtk_widget_show(new_item);
}

static void remove_tree_item(GtkWidget* w, GtkTree* tree) {
    GList* selected, *clear_list;
    GtkTree* root_tree;

    root_tree = GTK_TREE_ROOT_TREE(tree);
    selected = GTK_TREE_SELECTION(tree);

    clear_list = NULL;
    
    while (selected) {
	clear_list = g_list_prepend (clear_list, selected->data);
	selected = selected->next;
    }

    if(clear_list) {
	clear_list = g_list_reverse (clear_list);
	gtk_tree_remove_items(root_tree, clear_list);

	selected = clear_list;
	
	while (selected) {
	    gtk_widget_destroy (GTK_WIDGET (selected->data));
	    selected = selected->next;
	}

	g_list_free (clear_list);
    }
}

void create_tree_item(GtkWidget* parent, int level, int nb_item, int level_max) {
    int i;
    char buffer[255];
    GtkWidget *item, *tree;
    
    for(i = 0; i<nb_item; i++) { 

	sprintf(buffer, "item %d-%d", level, i);
	item = gtk_tree_item_new_with_label(buffer);
	gtk_tree_append(GTK_TREE(parent), item);
	gtk_widget_show(item);

/* 	g_print("item '%s' : 0x%x\n", buffer, (int)item); */

	if(level < level_max) {
	    tree = gtk_tree_new();

/* 	    g_print("subtree '%s' : 0x%x\n", buffer, (int)tree); */
	    gtk_signal_connect(GTK_OBJECT(tree), "selection_changed",
			       (GtkSignalFunc)cb_tree_changed,
			       (gpointer)NULL);
	    create_tree_item(tree, level+1, nb_item, level_max);
	    gtk_tree_item_set_subtree(GTK_TREE_ITEM(item), tree);
/* 	    gtk_tree_item_expand(GTK_TREE_ITEM(item)); */

	}
    }

}

void create_tree_page(GtkWidget* parent, GtkSelectionMode mode, 
		      char* page_name) {
    GtkWidget *root, *scrolled_win;
    GtkWidget *box, *label;
    GtkWidget *button;
    sTreeButton* tree_buttons;

    /* create notebook page */
    box = gtk_vbox_new(FALSE, 5);
    gtk_container_border_width (GTK_CONTAINER (box), 5);
    gtk_widget_show (box);

    label = gtk_label_new(page_name);
    gtk_notebook_append_page(GTK_NOTEBOOK(parent), box, label);

    scrolled_win = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (box), scrolled_win, TRUE, TRUE, 0);
    gtk_widget_set_usize (scrolled_win, 200, 200);
    gtk_widget_show (scrolled_win);

    root = gtk_tree_new();
/*     g_print("root: 0x%x\n", (int)root); */
    gtk_container_add(GTK_CONTAINER(scrolled_win), root);
    gtk_tree_set_selection_mode(GTK_TREE(root), mode);
/*     gtk_tree_set_view_mode(GTK_TREE(root), GTK_TREE_VIEW_ITEM); */
    gtk_signal_connect(GTK_OBJECT(root), "selection_changed",
		       (GtkSignalFunc)cb_tree_changed,
		       (gpointer)NULL);
    gtk_widget_show(root);
    
    create_tree_item(root, 1, 3, 3);

    tree_buttons = g_malloc(sizeof(sTreeButton));

    button = gtk_button_new_with_label("Add");
    gtk_box_pack_start(GTK_BOX (box), button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
		       (GtkSignalFunc)add_tree_item, 
		       (gpointer)root);
    gtk_widget_set_sensitive(button, FALSE);
    gtk_widget_show(button);
    tree_buttons->button_add = button;

    button = gtk_button_new_with_label("Remove");
    gtk_box_pack_start(GTK_BOX (box), button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
		       (GtkSignalFunc)remove_tree_item, 
		       (gpointer)root);
    gtk_widget_set_sensitive(button, FALSE);
    gtk_widget_show(button);
    tree_buttons->button_remove = button;

    gtk_object_set_user_data(GTK_OBJECT(root), (gpointer)tree_buttons);
}

void main(int argc, char** argv) {
    GtkWidget* window, *notebook;
    GtkWidget* box1;
    GtkWidget* separator;
    GtkWidget* button;

    gtk_init (&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Test Tree");
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                        (GtkSignalFunc) cb_delete_event, NULL);
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
                        (GtkSignalFunc) cb_destroy_event, NULL);
    box1 = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), box1);
    gtk_widget_show(box1);

    /* create notebook */
    notebook = gtk_notebook_new ();
    gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);
    gtk_box_pack_start (GTK_BOX (box1), notebook, TRUE, TRUE, 0);
    gtk_widget_show (notebook);

    /* create unique selection page */
    create_tree_page(notebook, GTK_SELECTION_SINGLE, "Single");
    create_tree_page(notebook, GTK_SELECTION_BROWSE, "Browse");
    create_tree_page(notebook, GTK_SELECTION_MULTIPLE, "Multiple");

    separator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX (box1), separator, TRUE, TRUE, 0);
    gtk_widget_show (separator);

    button = gtk_button_new_with_label("Close");
    gtk_box_pack_start(GTK_BOX (box1), button, TRUE, TRUE, 0);
    gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
			      (GtkSignalFunc)gtk_widget_destroy, 
			      GTK_OBJECT(window));
    gtk_widget_show(button);

    gtk_widget_show(window);

    gtk_main();
}
