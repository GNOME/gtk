#include <gtk/gtk.h>

typedef struct _TreeStruct TreeStruct;
struct _TreeStruct
{
  const gchar *label;
  gboolean alex;
  gboolean havoc;
  gboolean tim;
  gboolean owen;
  gboolean dave;
  gboolean world_holiday; /* shared by the european hackers */
  TreeStruct *children;
};


static TreeStruct january[] =
{
  {"New Years Day", TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, NULL },
  {"Presidential Inauguration", FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, NULL },
  {"Martin Luther King Jr. day", FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, NULL },
  { NULL }
};

static TreeStruct february[] =
{
  { "Presidents' Day", FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, NULL },
  { "Groundhog Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Valentine's Day", FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, NULL },
  { NULL }
};

static TreeStruct march[] =
{
  { "National Tree Planting Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "St Patrick's Day", FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, NULL },
  { NULL }
};

static TreeStruct april[] =
{
  { "April Fools' Day", FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, NULL },
  { "Army Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Earth Day", FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, NULL },
  { "Administrative Professionals' Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { NULL }
};

static TreeStruct may[] =
{
  { "Nurses' Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "National Day of Prayer", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Mothers' Day", FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, NULL },
  { "Armed Forces Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Memorial Day", TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, NULL },
  { NULL }
};

static TreeStruct june[] =
{
  { "June Fathers' Day", FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, NULL },
  { "Juneteenth (Liberation of Slaves)", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Flag Day", FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, NULL },
  { NULL }
};

static TreeStruct july[] =
{
  { "Parents' Day", FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, NULL },
  { "Independence Day", FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, NULL },
  { NULL }
};

static TreeStruct august[] =
{
  { "Air Force Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Coast Guard Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Friendship Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { NULL }
};

static TreeStruct september[] =
{
  { "Grandparents' Day", FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, NULL },
  { "Citizenship Day or Constitution Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Labor Day", TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, NULL },
  { NULL }
};

static TreeStruct october[] =
{
  { "National Children's Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Bosses' Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Sweetest Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Mother-in-Law's Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Navy Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Columbus Day", FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, NULL },
  { "Halloween", FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, NULL },
  { NULL }
};

static TreeStruct november[] =
{
  { "Marine Corps Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Veterans' Day", TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, NULL },
  { "Thanksgiving", FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, NULL },
  { NULL }
};

static TreeStruct december[] =
{
  { "Pearl Harbor Remembrance Day", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { "Christmas", TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, NULL },
  { "Kwanzaa", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL },
  { NULL }
};


static TreeStruct toplevel[] =
{
  {"January", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, january},
  {"February", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, february},
  {"March", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, march},
  {"April", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, april},
  {"May", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, may},
  {"June", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, june},
  {"July", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, july},
  {"August", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, august},
  {"September", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, september},
  {"October", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, october},
  {"November", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, november},
  {"December", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, december},
  {NULL}
};


enum
{
  HOLIDAY_COLUMN = 0,
  ALEX_COLUMN,
  HAVOC_COLUMN,
  TIM_COLUMN,
  OWEN_COLUMN,
  DAVE_COLUMN,
  VISIBLE_COLUMN,
  WORLD_COLUMN,
  NUM_COLUMNS
};

static GtkTreeModel *
make_model (void)
{
  GtkTreeStore *model;
  TreeStruct *month = toplevel;
  GtkTreeIter iter;

  model = gtk_tree_store_new_with_types (NUM_COLUMNS,
					 G_TYPE_STRING,
					 G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN);

  while (month->label)
    {
      TreeStruct *holiday = month->children;

      gtk_tree_store_append (model, &iter, NULL);
      gtk_tree_store_set (model, &iter,
			  HOLIDAY_COLUMN, month->label,
			  ALEX_COLUMN, FALSE,
			  HAVOC_COLUMN, FALSE,
			  TIM_COLUMN, FALSE,
			  OWEN_COLUMN, FALSE,
			  DAVE_COLUMN, FALSE,
			  VISIBLE_COLUMN, FALSE,
			  WORLD_COLUMN, FALSE,
			  -1);
      while (holiday->label)
	{
	  GtkTreeIter child_iter;

	  gtk_tree_store_append (model, &child_iter, &iter);
	  gtk_tree_store_set (model, &child_iter,
			      HOLIDAY_COLUMN, holiday->label,
			      ALEX_COLUMN, holiday->alex,
			      HAVOC_COLUMN, holiday->havoc,
			      TIM_COLUMN, holiday->tim,
			      OWEN_COLUMN, holiday->owen,
			      DAVE_COLUMN, holiday->dave,
			      VISIBLE_COLUMN, TRUE,
			      WORLD_COLUMN, holiday->world_holiday,
			      -1);

	  holiday ++;
	}
      month ++;
    }

  return GTK_TREE_MODEL (model);
}

static void
alex_toggled (GtkCellRendererToggle *cell,
	      gchar                 *path_str,
	      gpointer               data)
{
  GtkTreeModel *model = (GtkTreeModel *) data;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  gboolean alex;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, ALEX_COLUMN, &alex, -1);

  alex = !alex;
  gtk_tree_store_set (GTK_TREE_STORE (model), &iter, ALEX_COLUMN, alex, -1);

  gtk_tree_path_free (path);
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

static void
tim_toggled (GtkCellRendererToggle *cell,
	     gchar                 *path_str,
	     gpointer               data)
{
  GtkTreeModel *model = (GtkTreeModel *) data;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  gboolean tim;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, TIM_COLUMN, &tim, -1);

  tim = !tim;
  gtk_tree_store_set (GTK_TREE_STORE (model), &iter, TIM_COLUMN, tim, -1);

  gtk_tree_path_free (path);
}

static void
dave_toggled (GtkCellRendererToggle *cell,
	      gchar                 *path_str,
	      gpointer               data)
{
  GtkTreeModel *model = (GtkTreeModel *) data;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  gboolean dave;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, DAVE_COLUMN, &dave, -1);

  dave = !dave;
  gtk_tree_store_set (GTK_TREE_STORE (model), &iter, DAVE_COLUMN, dave, -1);

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
  gint col_offset;
  GtkTreeViewColumn *column;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Card planning sheet");
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
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree_view), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Holiday",
					       renderer,
					       "text", HOLIDAY_COLUMN, NULL);
  g_object_unref (renderer);

  /* Alex Column */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect_data (G_OBJECT (renderer), "toggled", alex_toggled, model, NULL, FALSE, FALSE);

  g_object_set (G_OBJECT (renderer), "xalign", 0.0, NULL);
  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
							    -1, "Alex",
							    renderer,
							    "active", ALEX_COLUMN,
							    "visible", VISIBLE_COLUMN,
							    "can_activate", WORLD_COLUMN,
							    NULL);
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), col_offset - 1);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column), GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_width (GTK_TREE_VIEW_COLUMN (column), 50);
  g_object_unref (renderer);

  /* Havoc Column */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect_data (G_OBJECT (renderer), "toggled", havoc_toggled, model, NULL, FALSE, FALSE);

  g_object_set (G_OBJECT (renderer), "xalign", 0.0, NULL);
  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
							    -1, "Havoc",
							    renderer,
							    "active", HAVOC_COLUMN,
							    "visible", VISIBLE_COLUMN,
							    NULL);
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), col_offset - 1);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column), GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_width (GTK_TREE_VIEW_COLUMN (column), 50);
  g_object_unref (renderer);

  /* Tim Column */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect_data (G_OBJECT (renderer), "toggled", tim_toggled, model, NULL, FALSE, FALSE);

  g_object_set (G_OBJECT (renderer), "xalign", 0.0, NULL);
  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Tim",
					       renderer,
					       "active", TIM_COLUMN,
					       "visible", VISIBLE_COLUMN,
					       "can_activate", WORLD_COLUMN,
					       NULL);
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), col_offset - 1);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column), GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_width (GTK_TREE_VIEW_COLUMN (column), 50);
  g_object_unref (renderer);

  /* Owen Column */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect_data (G_OBJECT (renderer), "toggled", owen_toggled, model, NULL, FALSE, FALSE);
  g_object_set (G_OBJECT (renderer), "xalign", 0.0, NULL);
  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Owen",
					       renderer,
					       "active", OWEN_COLUMN,
					       "visible", VISIBLE_COLUMN,
					       NULL);
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), col_offset - 1);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column), GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_width (GTK_TREE_VIEW_COLUMN (column), 50);
  g_object_unref (renderer);

  /* Owen Column */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect_data (G_OBJECT (renderer), "toggled", dave_toggled, model, NULL, FALSE, FALSE);
  g_object_set (G_OBJECT (renderer), "xalign", 0.0, NULL);
  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Dave",
					       renderer,
					       "active", DAVE_COLUMN,
					       "visible", VISIBLE_COLUMN,
					       NULL);
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), col_offset - 1);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column), GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_width (GTK_TREE_VIEW_COLUMN (column), 50);
  g_object_unref (renderer);

  gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);

  gtk_signal_connect (GTK_OBJECT (tree_view), "realize",
		      GTK_SIGNAL_FUNC (gtk_tree_view_expand_all),
		      NULL);
  gtk_window_set_default_size (GTK_WINDOW (window),
			       650, 400);
  gtk_widget_show_all (window);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Model");
  gtk_signal_connect (GTK_OBJECT (window), "destroy", gtk_main_quit, NULL);
  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_box_pack_start (GTK_BOX (vbox), gtk_label_new ("The model revealed"), FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);


  tree_view = gtk_tree_view_new_with_model (model);
  g_object_unref (G_OBJECT (model));
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree_view), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Holiday Column",
					       renderer,
					       "text", 0, NULL);
  g_object_unref (renderer);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Alex Column",
					       renderer,
					       "text", 1, NULL);
  g_object_unref (renderer);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Havoc Column",
					       renderer,
					       "text", 2, NULL);
  g_object_unref (renderer);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Tim Column",
					       renderer,
					       "text", 3, NULL);
  g_object_unref (renderer);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Owen Column",
					       renderer,
					       "text", 4, NULL);
  g_object_unref (renderer);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Dave Column",
					       renderer,
					       "text", 5, NULL);
  g_object_unref (renderer);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Visible Column",
					       renderer,
					       "text", 6, NULL);
  g_object_unref (renderer);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "World Holiday",
					       renderer,
					       "text", 7, NULL);
  g_object_unref (renderer);

  gtk_signal_connect (GTK_OBJECT (tree_view), "realize",
		      GTK_SIGNAL_FUNC (gtk_tree_view_expand_all),
		      NULL);
			   
  gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);


  gtk_window_set_default_size (GTK_WINDOW (window),
			       650, 400);

  gtk_widget_show_all (window);
  gtk_main ();

  return 0;
}

