#include <gtk/gtk.h>

typedef struct _TreeStruct TreeStruct;
struct _TreeStruct
{
  const gchar *label;
  gboolean havoc;
  gboolean owen;
  TreeStruct *children;
};

static TreeStruct january[] =
{
  {"New Years Day", TRUE, TRUE, NULL},
  {"Presidential Inauguration", TRUE, TRUE, NULL},
  {"Martin Luther King Jr. day", TRUE, TRUE, NULL},
  { NULL }
};

static TreeStruct february[] =
{
  { "Presidents' Day", TRUE, TRUE, NULL },
  { "Groundhog Day", FALSE, FALSE, NULL },
  { "Valentine's Day", FALSE, FALSE, NULL },
  { NULL }
};

static TreeStruct march[] =
{
  { "National Tree Planting Day", FALSE, FALSE, NULL },
  { "St Patrick's Day", FALSE, FALSE, NULL },
  { NULL }
};

static TreeStruct april[] =
{
  { "April Fools' Day", FALSE, FALSE, NULL },
  { "Army Day", FALSE, FALSE, NULL },
  { "Earth Day", FALSE, FALSE, NULL },
  { "Administrative Professionals' Day", FALSE, FALSE, NULL },
  { NULL }
};

static TreeStruct may[] =
{
  { "Nurses' Day", FALSE, FALSE, NULL },
  { "National Day of Prayer", FALSE, FALSE, NULL },
  { "Mothers' Day", FALSE, FALSE, NULL },
  { "Armed Forces Day", FALSE, FALSE, NULL },
  { "Memorial Day", TRUE, TRUE, NULL },
  { NULL }
};

static TreeStruct june[] =
{
  { "June Fathers' Day", FALSE, FALSE, NULL },
  { "Juneteenth (Liberation of Slaves)", FALSE, FALSE, NULL },
  { "Flag Day", TRUE, TRUE, NULL },
  { NULL }
};

static TreeStruct july[] =
{
  { "Parents' Day", FALSE, FALSE, NULL },
  { "Independence Day", TRUE, TRUE, NULL },
  { NULL }
};

static TreeStruct august[] =
{
  { "Air Force Day", FALSE, FALSE, NULL },
  { "Coast Guard Day", FALSE, FALSE, NULL },
  { "Friendship Day", FALSE, FALSE, NULL },
  { NULL }
};

static TreeStruct september[] =
{
  { "Grandparents' Day", FALSE, FALSE, NULL },
  { "Citizenship Day or Constitution Day", FALSE, FALSE, NULL },
  { "Labor Day", TRUE, TRUE, NULL },
  { NULL }
};

static TreeStruct october[] =
{
  { "National Children's Day", FALSE, FALSE, NULL },
  { "Bosses' Day", FALSE, FALSE, NULL },
  { "Sweetest Day", FALSE, FALSE, NULL },
  { "Mother-in-Law's Day", FALSE, FALSE, NULL },
  { "Navy Day", FALSE, FALSE, NULL },
  { "Columbus Day", TRUE, TRUE, NULL },
  { "Halloween", FALSE, FALSE, NULL },
  { NULL }
};

static TreeStruct november[] =
{
  { "Marine Corps Day", FALSE, FALSE, NULL },
  { "Veterans' Day", TRUE, TRUE, NULL },
  { "Thanksgiving", TRUE, TRUE, NULL },
  { NULL }
};

static TreeStruct december[] =
{
  { "Pearl Harbor Remembrance Day", FALSE, FALSE, NULL },
  { "Christmas", TRUE, TRUE, NULL },
  { "Kwanzaa", FALSE, FALSE, NULL },
  { NULL }
};


static TreeStruct toplevel[] =
{
  {"January", FALSE, FALSE, january},
  {"February", FALSE, FALSE, february},
  {"March", FALSE, FALSE, march},
  {"April", FALSE, FALSE, april},
  {"May", FALSE, FALSE, may},
  {"June", FALSE, FALSE, june},
  {"July", FALSE, FALSE, july},
  {"August", FALSE, FALSE, august},
  {"September", FALSE, FALSE, september},
  {"October", FALSE, FALSE, october},
  {"November", FALSE, FALSE, november},
  {"December", FALSE, FALSE, december},
  {NULL}
};


enum
{
  HOLIDAY_COLUMN = 0,
  HAVOC_COLUMN,
  OWEN_COLUMN,
  VISIBLE_COLUMN,
  NUM_COLUMNS
};

static GtkTreeModel *
make_model (void)
{
  GtkTreeStore *model;
  TreeStruct *month = toplevel;
  GtkTreeIter iter;

  model = gtk_tree_store_new_with_types (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);

  while (month->label)
    {
      TreeStruct *holiday = month->children;

      gtk_tree_store_append (model, &iter, NULL);
      gtk_tree_store_set (model, &iter,
			  HOLIDAY_COLUMN, month->label,
			  HAVOC_COLUMN, FALSE,
			  OWEN_COLUMN, FALSE,
			  VISIBLE_COLUMN, FALSE,
			  -1);
      while (holiday->label)
	{
	  GtkTreeIter child_iter;

	  gtk_tree_store_append (model, &child_iter, &iter);
	  gtk_tree_store_set (model, &child_iter,
			      HOLIDAY_COLUMN, holiday->label,
			      HAVOC_COLUMN, holiday->havoc,
			      OWEN_COLUMN, holiday->owen,
			      VISIBLE_COLUMN, TRUE,
			      -1);

	  holiday ++;
	}
      month ++;
    }

  return GTK_TREE_MODEL (model);
}

static void
havoc_toggled (GtkCellRendererToggle *cell,
	       gchar                 *path_str,
	       gpointer               data)
{
  GtkTreeModel *model = (GtkTreeModel *) data;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  gboolean havoc;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, HAVOC_COLUMN, &havoc, -1);

  havoc = !havoc;
  gtk_tree_store_set (GTK_TREE_STORE (model), &iter, HAVOC_COLUMN, havoc, -1);

  gtk_tree_path_free (path);
}

static void
owen_toggled (GtkCellRendererToggle *cell,
	       gchar                 *path_str,
	       gpointer               data)
{
  GtkTreeModel *model = (GtkTreeModel *) data;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  gboolean owen;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, OWEN_COLUMN, &owen, -1);

  owen = !owen;
  gtk_tree_store_set (GTK_TREE_STORE (model), &iter, OWEN_COLUMN, owen, -1);

  gtk_tree_path_free (path);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *scrolled_window;
  GtkWidget *tree_view;
  GtkTreeModel *model;
  GtkCellRenderer *renderer;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect (GTK_OBJECT (window), "destroy", gtk_main_quit, NULL);
  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_box_pack_start (GTK_BOX (vbox), gtk_label_new ("Jonathan's Holiday Card Planning Sheet"), FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);

  model = make_model ();
  tree_view = gtk_tree_view_new_with_model (model);
  //  g_object_set (G_OBJECT (tree_view), "model", model, NULL);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree_view), TRUE);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Holiday",
					       renderer,
					       "text", HOLIDAY_COLUMN, NULL);
  g_object_unref (renderer);
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect_data (G_OBJECT (renderer), "toggled", havoc_toggled, model, NULL, FALSE, FALSE);

  g_object_set (G_OBJECT (renderer), "xalign", 0.0, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Havoc",
					       renderer,
					       "active", HAVOC_COLUMN,
					       "visible", VISIBLE_COLUMN,
					       NULL);
  g_object_unref (renderer);
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect_data (G_OBJECT (renderer), "toggled", owen_toggled, model, NULL, FALSE, FALSE);
  g_object_set (G_OBJECT (renderer), "xalign", 0.0, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Owen",
					       renderer,
					       "active", OWEN_COLUMN,
					       "visible", VISIBLE_COLUMN,
					       NULL);
  g_object_unref (renderer);
  gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);


  gtk_widget_show_all (window);
  gtk_main ();

  return 0;
}

