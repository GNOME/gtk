
#include <gtk/gtk.h>
#include <string.h>


/* Don't copy this bad example; inline RGB data is always a better
 * idea than inline XPMs.
 */
static char  *book_closed_xpm[] = {
"16 16 6 1",
"       c None s None",
".      c black",
"X      c red",
"o      c yellow",
"O      c #808080",
"#      c white",
"                ",
"       ..       ",
"     ..XX.      ",
"   ..XXXXX.     ",
" ..XXXXXXXX.    ",
".ooXXXXXXXXX.   ",
"..ooXXXXXXXXX.  ",
".X.ooXXXXXXXXX. ",
".XX.ooXXXXXX..  ",
" .XX.ooXXX..#O  ",
"  .XX.oo..##OO. ",
"   .XX..##OO..  ",
"    .X.#OO..    ",
"     ..O..      ",
"      ..        ",
"                "
};

static GtkWidget* create_prop_editor (GObject *object);
static void run_automated_tests (void);

/* This custom model is to test custom model use. */

#define GTK_TYPE_MODEL_TYPES			(gtk_tree_model_types_get_type ())
#define GTK_TREE_MODEL_TYPES(obj)		(GTK_CHECK_CAST ((obj), GTK_TYPE_MODEL_TYPES, GtkTreeModelTypes))
#define GTK_TREE_MODEL_TYPES_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_MODEL_TYPES, GtkTreeModelTypesClass))
#define GTK_IS_TREE_MODEL_TYPES(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_MODEL_TYPES))
#define GTK_IS_TREE_MODEL_TYPES_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_MODEL_TYPES))

typedef struct _GtkTreeModelTypes       GtkTreeModelTypes;
typedef struct _GtkTreeModelTypesClass  GtkTreeModelTypesClass;

struct _GtkTreeModelTypes
{
  GtkObject parent;

  gint stamp;
};

struct _GtkTreeModelTypesClass
{
  GtkObjectClass parent_class;

  guint        (* get_flags)       (GtkTreeModel *tree_model);   
  gint         (* get_n_columns)   (GtkTreeModel *tree_model);
  GType        (* get_column_type) (GtkTreeModel *tree_model,
				    gint          index);
  gboolean     (* get_iter)        (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    GtkTreePath  *path);
  GtkTreePath *(* get_path)        (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);
  void         (* get_value)       (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    gint          column,
				    GValue       *value);
  gboolean     (* iter_next)       (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);
  gboolean     (* iter_children)   (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    GtkTreeIter  *parent);
  gboolean     (* iter_has_child)  (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);
  gint         (* iter_n_children) (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);
  gboolean     (* iter_nth_child)  (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    GtkTreeIter  *parent,
				    gint          n);
  gboolean     (* iter_parent)     (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter,
				    GtkTreeIter  *child);
  void         (* ref_iter)        (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);
  void         (* unref_iter)      (GtkTreeModel *tree_model,
				    GtkTreeIter  *iter);

  /* These will be moved into the GtkTreeModelIface eventually */
  void         (* changed)         (GtkTreeModel *tree_model,
				    GtkTreePath  *path,
				    GtkTreeIter  *iter);
  void         (* inserted)        (GtkTreeModel *tree_model,
				    GtkTreePath  *path,
				    GtkTreeIter  *iter);
  void         (* child_toggled)   (GtkTreeModel *tree_model,
				    GtkTreePath  *path,
				    GtkTreeIter  *iter);
  void         (* deleted)         (GtkTreeModel *tree_model,
				    GtkTreePath  *path);
};

GtkType             gtk_tree_model_types_get_type      (void);
GtkTreeModelTypes *gtk_tree_model_types_new           (void);

typedef enum
{
  COLUMNS_NONE,
  COLUMNS_ONE,
  COLUMNS_LOTS,
  COLUMNS_LAST
} ColumnsType;

static gchar *column_type_names[] = {
  "No columns",
  "One column",
  "Many columns"
};

#define N_COLUMNS 9

static GType*
get_model_types (void)
{
  static GType column_types[N_COLUMNS] = { 0 };
  
  if (column_types[0] == 0)
    {
      column_types[0] = G_TYPE_STRING;
      column_types[1] = G_TYPE_STRING;
      column_types[2] = GDK_TYPE_PIXBUF;
      column_types[3] = G_TYPE_FLOAT;
      column_types[4] = G_TYPE_UINT;
      column_types[5] = G_TYPE_UCHAR;
      column_types[6] = G_TYPE_CHAR;
#define BOOL_COLUMN 7
      column_types[BOOL_COLUMN] = G_TYPE_BOOLEAN;
      column_types[8] = G_TYPE_INT;
    }

  return column_types;
}

static void
col_clicked_cb (GtkTreeViewColumn *col, gpointer data)
{
  GtkWindow *win;

  win = GTK_WINDOW (create_prop_editor (G_OBJECT (col)));

  gtk_window_set_title (win, gtk_tree_view_column_get_title (col));
}

static void
setup_column (GtkTreeViewColumn *col)
{
  g_signal_connect_data (G_OBJECT (col),
                         "clicked",
                         (GCallback) col_clicked_cb,
                         NULL,
                         NULL,
                         FALSE,
                         FALSE);
}

static void
toggled_callback (GtkCellRendererToggle *celltoggle,
                  gchar                 *path_string,
                  GtkTreeView           *tree_view)
{
  GtkTreeModel *model = NULL;
  GtkTreeModelSort *sort_model = NULL;
  GtkTreePath *path;
  GtkTreeIter iter;
  gboolean active = FALSE;
  
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  model = gtk_tree_view_get_model (tree_view);
  
  if (GTK_IS_TREE_MODEL_SORT (model))
    {
      sort_model = GTK_TREE_MODEL_SORT (model);
      model = gtk_tree_model_sort_get_model (sort_model);
    }

  if (model == NULL)
    return;

  if (sort_model)
    {
      g_warning ("FIXME implement conversion from TreeModelSort iter to child model iter");
      return;
    }
      
  path = gtk_tree_path_new_from_string (path_string);
  if (!gtk_tree_model_get_iter (model,
                                &iter, path))
    {
      g_warning ("%s: bad path?", G_STRLOC);
      return;
    }
  gtk_tree_path_free (path);
  
  if (GTK_IS_LIST_STORE (model))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (model),
                          &iter,
                          BOOL_COLUMN,
                          &active,
                          -1);
      
      gtk_list_store_set (GTK_LIST_STORE (model),
                          &iter,
                          BOOL_COLUMN,
                          !active,
                          -1);
    }
  else if (GTK_IS_TREE_STORE (model))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (model),
                          &iter,
                          BOOL_COLUMN,
                          &active,
                          -1);
            
      gtk_tree_store_set (GTK_TREE_STORE (model),
                          &iter,
                          BOOL_COLUMN,
                          !active,
                          -1);
    }
  else
    g_warning ("don't know how to actually toggle value for model type %s",
               g_type_name (G_TYPE_FROM_INSTANCE (model)));
}


static ColumnsType current_column_type = COLUMNS_LOTS;

static void
set_columns_type (GtkTreeView *tree_view, ColumnsType type)
{
  GtkTreeViewColumn *col;
  GtkCellRenderer *rend;
  GdkPixbuf *pixbuf;
  GtkWidget *image;
  
  current_column_type = type;
  
  col = gtk_tree_view_get_column (tree_view, 0);
  while (col)
    {
      gtk_tree_view_remove_column (tree_view, col);

      col = gtk_tree_view_get_column (tree_view, 0);
    }

  gtk_tree_view_set_rules_hint (tree_view, FALSE);
  
  switch (type)
    {
    case COLUMNS_NONE:
      break;

    case COLUMNS_LOTS:
      /* with lots of columns we need to turn on rules */
      gtk_tree_view_set_rules_hint (tree_view, TRUE);
      
      rend = gtk_cell_renderer_text_new ();
      
      col = gtk_tree_view_column_new_with_attributes ("Column 1",
                                                      rend,
                                                      "text", 1,
                                                      NULL);
      setup_column (col);
      
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), col);
      
      g_object_unref (G_OBJECT (rend));
      g_object_unref (G_OBJECT (col));

      rend = gtk_cell_renderer_text_pixbuf_new ();
      
      col = gtk_tree_view_column_new_with_attributes ("Column 2",
                                                      rend,
                                                      "text", 0,
                                                      "pixbuf", 2,
                                                      NULL);

      setup_column (col);
      
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), col);
      
      g_object_unref (G_OBJECT (rend));
      g_object_unref (G_OBJECT (col));

      rend = gtk_cell_renderer_toggle_new ();

      g_signal_connect_data (G_OBJECT (rend), "toggled",
                             toggled_callback, tree_view,
                             NULL, FALSE, FALSE);
      
      col = gtk_tree_view_column_new_with_attributes ("Column 3",
                                                      rend,
                                                      "active", BOOL_COLUMN,
                                                      NULL);

      setup_column (col);
      
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), col);

      pixbuf = gdk_pixbuf_new_from_xpm_data (book_closed_xpm);

      image = gtk_image_new_from_pixbuf (pixbuf);

      g_object_unref (G_OBJECT (pixbuf));
      
      gtk_widget_show (image);
      
      gtk_tree_view_column_set_widget (col, image);
      
      g_object_unref (G_OBJECT (rend));
      g_object_unref (G_OBJECT (col));

      rend = gtk_cell_renderer_toggle_new ();

      /* you could also set this per-row by tying it to a column
       * in the model of course.
       */
      g_object_set (G_OBJECT (rend), "radio", TRUE, NULL);
      
      g_signal_connect_data (G_OBJECT (rend), "toggled",
                             toggled_callback, tree_view,
                             NULL, FALSE, FALSE);
      
      col = gtk_tree_view_column_new_with_attributes ("Column 4",
                                                      rend,
                                                      "active", BOOL_COLUMN,
                                                      NULL);

      setup_column (col);
      
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), col);
      
      g_object_unref (G_OBJECT (rend));
      g_object_unref (G_OBJECT (col));

#if 0

      rend = gtk_cell_renderer_text_new ();
      
      col = gtk_tree_view_column_new_with_attributes ("Column 5",
                                                      rend,
                                                      "text", 3,
                                                      NULL);

      setup_column (col);
      
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), col);
      
      g_object_unref (G_OBJECT (rend));
      g_object_unref (G_OBJECT (col));


      rend = gtk_cell_renderer_text_new ();
      
      col = gtk_tree_view_column_new_with_attributes ("Column 6",
                                                      rend,
                                                      "text", 4,
                                                      NULL);

      setup_column (col);
      
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), col);
      
      g_object_unref (G_OBJECT (rend));
      g_object_unref (G_OBJECT (col));


      rend = gtk_cell_renderer_text_new ();
      
      col = gtk_tree_view_column_new_with_attributes ("Column 7",
                                                      rend,
                                                      "text", 5,
                                                      NULL);

      setup_column (col);
      
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), col);
      
      g_object_unref (G_OBJECT (rend));
      g_object_unref (G_OBJECT (col));

      rend = gtk_cell_renderer_text_new ();
      
      col = gtk_tree_view_column_new_with_attributes ("Column 8",
                                                      rend,
                                                      "text", 6,
                                                      NULL);

      setup_column (col);
      
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), col);
      
      g_object_unref (G_OBJECT (rend));
      g_object_unref (G_OBJECT (col));


      rend = gtk_cell_renderer_text_new ();
      
      col = gtk_tree_view_column_new_with_attributes ("Column 9",
                                                      rend,
                                                      "text", 7,
                                                      NULL);

      setup_column (col);
      
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), col);
      
      g_object_unref (G_OBJECT (rend));
      g_object_unref (G_OBJECT (col));
      

      rend = gtk_cell_renderer_text_new ();
      
      col = gtk_tree_view_column_new_with_attributes ("Column 10",
                                                      rend,
                                                      "text", 8,
                                                      NULL);

      setup_column (col);
      
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), col);
      
      g_object_unref (G_OBJECT (rend));
      g_object_unref (G_OBJECT (col));

#endif
      
      gtk_tree_view_set_expander_column (tree_view, 1);
      
      /* FALL THRU */
      
    case COLUMNS_ONE:
      rend = gtk_cell_renderer_text_new ();
      
      col = gtk_tree_view_column_new_with_attributes ("Column 0",
                                                      rend,
                                                      "text", 0,
                                                      NULL);

      setup_column (col);
      
      gtk_tree_view_insert_column (GTK_TREE_VIEW (tree_view), col, 0);
      
      g_object_unref (G_OBJECT (rend));
      g_object_unref (G_OBJECT (col));
      
    default:
      break;
    }
}

static ColumnsType
get_columns_type (void)
{
  return current_column_type;
}

static GdkPixbuf *our_pixbuf;
  
typedef enum
{
  /*   MODEL_TYPES, */
  MODEL_TREE,
  MODEL_LIST,
  MODEL_SORTED_TREE,
  MODEL_SORTED_LIST,
  MODEL_EMPTY_LIST,
  MODEL_EMPTY_TREE,
  MODEL_NULL,
  MODEL_LAST
} ModelType;

/* FIXME add a custom model to test */
static GtkTreeModel *models[MODEL_LAST];
static const char *model_names[MODEL_LAST] = {
  "GtkTreeStore",
  "GtkListStore",
  "GtkTreeModelSort wrapping GtkTreeStore",
  "GtkTreeModelSort wrapping GtkListStore",
  "Empty GtkListStore",
  "Empty GtkTreeStore",
  "NULL (no model)"
};

static GtkTreeModel*
create_list_model (void)
{
  GtkListStore *store;
  GtkTreeIter iter;
  gint i;
  GType *t;

  t = get_model_types ();
  
  store = gtk_list_store_new_with_types (N_COLUMNS,
                                         t[0], t[1], t[2],
                                         t[3], t[4], t[5],
                                         t[6], t[7], t[8]);

  i = 0;
  while (i < 200)
    {
      char *msg;
      
      gtk_list_store_append (store, &iter);

      msg = g_strdup_printf ("%d", i);
      
      gtk_list_store_set (store, &iter, 0, msg, 1, "Foo! Foo! Foo!",
                          2, our_pixbuf,
                          3, 7.0, 4, (guint) 9000,
                          5, 'f', 6, 'g',
                          7, TRUE, 8, 23245454,
                          -1);

      g_free (msg);
      
      ++i;
    }

  return GTK_TREE_MODEL (store);
}

static void
typesystem_recurse (GType        type,
                    GtkTreeIter *parent_iter,
                    GtkTreeStore *store)
{
  GType* children;
  guint n_children = 0;
  gint i;
  GtkTreeIter iter;
  gchar *str;
  
  gtk_tree_store_append (store, &iter, parent_iter);

  str = g_strdup_printf ("%d", type);
  gtk_tree_store_set (store, &iter, 0, str, 1, g_type_name (type),
                      2, our_pixbuf,
                      3, 7.0, 4, (guint) 9000,
                      5, 'f', 6, 'g',
                      7, TRUE, 8, 23245454,
                      -1);
  g_free (str);
  
  children = g_type_children (type, &n_children);

  i = 0;
  while (i < n_children)
    {
      typesystem_recurse (children[i], &iter, store);

      ++i;
    }
  
  g_free (children);
}

static GtkTreeModel*
create_tree_model (void)
{
  GtkTreeStore *store;
  gint i;
  GType *t;
  volatile GType dummy; /* G_GNUC_CONST makes the optimizer remove
                         * get_type calls if you don't do something
                         * like this
                         */
  
  /* Make the tree more interesting */
  dummy = gtk_scrolled_window_get_type ();
  dummy = gtk_label_get_type ();
  dummy = gtk_hscrollbar_get_type ();
  dummy = gtk_vscrollbar_get_type ();
  dummy = pango_layout_get_type ();

  t = get_model_types ();
  
  store = gtk_tree_store_new_with_types (N_COLUMNS,
                                         t[0], t[1], t[2],
                                         t[3], t[4], t[5],
                                         t[6], t[7], t[8]);

  i = 0;
  while (i < G_TYPE_LAST_RESERVED_FUNDAMENTAL)
    {
      typesystem_recurse (i, NULL, store);
      
      ++i;
    }

  return GTK_TREE_MODEL (store);
}

static void
model_selected (GtkOptionMenu *om, gpointer data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (data);
  gint hist;

  hist = gtk_option_menu_get_history (om);

  if (models[hist] != gtk_tree_view_get_model (tree_view))
    {
      gtk_tree_view_set_model (tree_view, models[hist]);
    }
}

static void
columns_selected (GtkOptionMenu *om, gpointer data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (data);
  gint hist;

  hist = gtk_option_menu_get_history (om);

  if (hist != get_columns_type ())
    {
      set_columns_type (tree_view, hist);
    }
}


enum
{
  TARGET_GTK_TREE_MODEL_ROW
};

static GtkTargetEntry row_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_APP,
    TARGET_GTK_TREE_MODEL_ROW }
};

int
main (int    argc,
      char **argv)
{
  GtkWidget *window;
  GtkWidget *sw;
  GtkWidget *tv;
  GtkWidget *table;
  GtkWidget *om;
  GtkWidget *menu;
  GtkTreeModel *model;
  gint i;
  
  gtk_init (&argc, &argv);

  our_pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) book_closed_xpm);  
  
#if 0
  models[MODEL_TYPES] = GTK_TREE_MODEL (gtk_tree_model_types_new ());
#endif
  models[MODEL_LIST] = create_list_model ();
  models[MODEL_TREE] = create_tree_model ();

  model = create_list_model ();
  models[MODEL_SORTED_LIST] = gtk_tree_model_sort_new_with_model (model, NULL, 0);
  g_object_unref (G_OBJECT (model));

  model = create_tree_model ();
  models[MODEL_SORTED_TREE] = gtk_tree_model_sort_new_with_model (model, NULL, 0);
  g_object_unref (G_OBJECT (model));

  models[MODEL_EMPTY_LIST] = GTK_TREE_MODEL (gtk_list_store_new ());
  models[MODEL_EMPTY_TREE] = GTK_TREE_MODEL (gtk_tree_store_new ());
  
  models[MODEL_NULL] = NULL;

  run_automated_tests ();
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);

  table = gtk_table_new (3, 1, FALSE);

  gtk_container_add (GTK_CONTAINER (window), table);

  tv = gtk_tree_view_new_with_model (models[0]);
  
  gtk_tree_view_set_rows_drag_source (GTK_TREE_VIEW (tv),
                                      GDK_BUTTON1_MASK,
                                      row_targets,
                                      G_N_ELEMENTS (row_targets),
                                      GDK_ACTION_MOVE | GDK_ACTION_COPY,
                                      NULL, NULL);

  gtk_tree_view_set_rows_drag_dest (GTK_TREE_VIEW (tv),
                                    row_targets,
                                    G_N_ELEMENTS (row_targets),
                                    GDK_ACTION_MOVE | GDK_ACTION_COPY,
                                    NULL, NULL);
  
  /* Model menu */

  menu = gtk_menu_new ();
  
  i = 0;
  while (i < MODEL_LAST)
    {
      GtkWidget *mi;
      const char *name;

      name = model_names[i];
      
      mi = gtk_menu_item_new_with_label (name);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

#if 0
      window = create_prop_editor (G_OBJECT (models[i]));

      gtk_window_set_title (GTK_WINDOW (window),                            
                            name);
#endif

      ++i;
    }
  gtk_widget_show_all (menu);
  
  om = gtk_option_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (om), menu);
  
  gtk_table_attach (GTK_TABLE (table), om,
                    0, 1, 0, 1,
                    0, 0, 
                    0, 0);

  gtk_signal_connect (GTK_OBJECT (om),
                      "changed",
                      GTK_SIGNAL_FUNC (model_selected),
                      tv);
  
  /* Columns menu */

  menu = gtk_menu_new ();
  
  i = 0;
  while (i < COLUMNS_LAST)
    {
      GtkWidget *mi;
      const char *name;

      name = column_type_names[i];
      
      mi = gtk_menu_item_new_with_label (name);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

      ++i;
    }
  gtk_widget_show_all (menu);
  
  om = gtk_option_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (om), menu);
  
  gtk_table_attach (GTK_TABLE (table), om,
                    0, 1, 1, 2,
                    0, 0, 
                    0, 0);

  set_columns_type (GTK_TREE_VIEW (tv), COLUMNS_LOTS);
  gtk_option_menu_set_history (GTK_OPTION_MENU (om), COLUMNS_LOTS);
  
  gtk_signal_connect (GTK_OBJECT (om),
                      "changed",
                      GTK_SIGNAL_FUNC (columns_selected),
                      tv);
  
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  
  gtk_table_attach (GTK_TABLE (table), sw,
                    0, 1, 2, 3,
                    GTK_EXPAND | GTK_FILL,
                    GTK_EXPAND | GTK_FILL,
                    0, 0);
  
  gtk_container_add (GTK_CONTAINER (sw), tv);
  
  gtk_widget_show_all (window);
  
  gtk_main ();

  return 0;
}

/*
 * GtkTreeModelTypes
 */

enum {
  CHANGED,
  INSERTED,
  CHILD_TOGGLED,
  DELETED,

  LAST_SIGNAL
};

static void         gtk_tree_model_types_init                 (GtkTreeModelTypes      *model_types);
static void         gtk_tree_model_types_class_init           (GtkTreeModelTypesClass *class);
static void         gtk_tree_model_types_tree_model_init      (GtkTreeModelIface   *iface);
static gint         gtk_real_model_types_get_n_columns   (GtkTreeModel        *tree_model);
static GType        gtk_real_model_types_get_column_type (GtkTreeModel        *tree_model,
							   gint                 index);
static GtkTreePath *gtk_real_model_types_get_path        (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter);
static void         gtk_real_model_types_get_value       (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter,
							   gint                 column,
							   GValue              *value);
static gboolean     gtk_real_model_types_iter_next       (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter);
static gboolean     gtk_real_model_types_iter_children   (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter,
							   GtkTreeIter         *parent);
static gboolean     gtk_real_model_types_iter_has_child  (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter);
static gint         gtk_real_model_types_iter_n_children (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter);
static gboolean     gtk_real_model_types_iter_nth_child  (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter,
							   GtkTreeIter         *parent,
							   gint                 n);
static gboolean     gtk_real_model_types_iter_parent     (GtkTreeModel        *tree_model,
							   GtkTreeIter         *iter,
							   GtkTreeIter         *child);


static guint model_types_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_tree_model_types_get_type (void)
{
  static GtkType model_types_type = 0;

  if (!model_types_type)
    {
      static const GTypeInfo model_types_info =
      {
        sizeof (GtkTreeModelTypesClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_tree_model_types_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkTreeModelTypes),
	0,
        (GInstanceInitFunc) gtk_tree_model_types_init
      };

      static const GInterfaceInfo tree_model_info =
      {
	(GInterfaceInitFunc) gtk_tree_model_types_tree_model_init,
	NULL,
	NULL
      };

      model_types_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkTreeModelTypes", &model_types_info, 0);
      g_type_add_interface_static (model_types_type,
				   GTK_TYPE_TREE_MODEL,
				   &tree_model_info);
    }

  return model_types_type;
}

GtkTreeModelTypes *
gtk_tree_model_types_new (void)
{
  GtkTreeModelTypes *retval;

  retval = GTK_TREE_MODEL_TYPES (g_object_new (GTK_TYPE_MODEL_TYPES, NULL));

  return retval;
}

static void
gtk_tree_model_types_class_init (GtkTreeModelTypesClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) class;

  model_types_signals[CHANGED] =
    g_signal_newc ("changed",
                   GTK_CLASS_TYPE (object_class),
                   G_SIGNAL_RUN_FIRST,
                   GTK_SIGNAL_OFFSET (GtkTreeModelTypesClass, changed),
                   NULL,
                   gtk_marshal_VOID__BOXED_BOXED,
                   G_TYPE_NONE, 2,
                   G_TYPE_POINTER,
                   G_TYPE_POINTER);
  model_types_signals[INSERTED] =
    g_signal_newc ("inserted",
                   GTK_CLASS_TYPE (object_class),
                   G_SIGNAL_RUN_FIRST,
                   GTK_SIGNAL_OFFSET (GtkTreeModelTypesClass, inserted),
                   NULL,
                   gtk_marshal_VOID__BOXED_BOXED,
                   G_TYPE_NONE, 2,
                   G_TYPE_POINTER,
                   G_TYPE_POINTER);
  model_types_signals[CHILD_TOGGLED] =
    g_signal_newc ("child_toggled",
                   GTK_CLASS_TYPE (object_class),
                   G_SIGNAL_RUN_FIRST,
                   GTK_SIGNAL_OFFSET (GtkTreeModelTypesClass, child_toggled),
                   NULL,
                   gtk_marshal_VOID__BOXED_BOXED,
                   G_TYPE_NONE, 2,
                   G_TYPE_POINTER,
                   G_TYPE_POINTER);
  model_types_signals[DELETED] =
    g_signal_newc ("deleted",
                   GTK_CLASS_TYPE (object_class),
                   G_SIGNAL_RUN_FIRST,
                   GTK_SIGNAL_OFFSET (GtkTreeModelTypesClass, deleted),
                   NULL,
                   gtk_marshal_VOID__BOXED,
                   G_TYPE_NONE, 1,
                   G_TYPE_POINTER);
}

static void
gtk_tree_model_types_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_n_columns = gtk_real_model_types_get_n_columns;
  iface->get_column_type = gtk_real_model_types_get_column_type;
  iface->get_path = gtk_real_model_types_get_path;
  iface->get_value = gtk_real_model_types_get_value;
  iface->iter_next = gtk_real_model_types_iter_next;
  iface->iter_children = gtk_real_model_types_iter_children;
  iface->iter_has_child = gtk_real_model_types_iter_has_child;
  iface->iter_n_children = gtk_real_model_types_iter_n_children;
  iface->iter_nth_child = gtk_real_model_types_iter_nth_child;
  iface->iter_parent = gtk_real_model_types_iter_parent;
}

static void
gtk_tree_model_types_init (GtkTreeModelTypes *model_types)
{
  model_types->stamp = g_random_int ();
}

static GType column_types[] = {
  G_TYPE_STRING, /* GType */
  G_TYPE_STRING  /* type name */
};
  
static gint
gtk_real_model_types_get_n_columns (GtkTreeModel *tree_model)
{
  return G_N_ELEMENTS (column_types);
}

static GType
gtk_real_model_types_get_column_type (GtkTreeModel *tree_model,
                                      gint          index)
{
  g_return_val_if_fail (index < G_N_ELEMENTS (column_types), G_TYPE_INVALID);
  
  return column_types[index];
}

#if 0
/* Use default implementation of this */
static gboolean
gtk_real_model_types_get_iter (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter,
                               GtkTreePath  *path)
{
  
}
#endif

/* The toplevel nodes of the tree are the reserved types, G_TYPE_NONE through
 * G_TYPE_RESERVED_FUNDAMENTAL.
 */

static GtkTreePath *
gtk_real_model_types_get_path (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter)
{
  GtkTreePath *retval;
  GType type;
  GType parent;
  
  g_return_val_if_fail (GTK_IS_TREE_MODEL_TYPES (tree_model), NULL);
  g_return_val_if_fail (iter != NULL, NULL);

  type = GPOINTER_TO_INT (iter->user_data);
  
  retval = gtk_tree_path_new ();
  
  parent = g_type_parent (type);
  while (parent != G_TYPE_INVALID)
    {
      GType* children = g_type_children (parent, NULL);
      gint i = 0;

      if (!children || children[0] == G_TYPE_INVALID)
        {
          g_warning ("bad iterator?");
          return NULL;
        }
      
      while (children[i] != type)
        ++i;

      gtk_tree_path_prepend_index (retval, i);

      g_free (children);
      
      type = parent;
      parent = g_type_parent (parent);
    }

  /* The fundamental type itself is the index on the toplevel */
  gtk_tree_path_prepend_index (retval, type);

  return retval;
}

static void
gtk_real_model_types_get_value (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter,
                                gint          column,
                                GValue       *value)
{
  GType type;

  type = GPOINTER_TO_INT (iter->user_data);

  switch (column)
    {
    case 0:
      {
        gchar *str;
        
        g_value_init (value, G_TYPE_STRING);

        str = g_strdup_printf ("%d", type);
        g_value_set_string (value, str);
        g_free (str);
      }
      break;

    case 1:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_string (value, g_type_name (type));
      break;

    default:
      g_warning ("Bad column %d requested", column);
    }
}

static gboolean
gtk_real_model_types_iter_next (GtkTreeModel  *tree_model,
                                GtkTreeIter   *iter)
{
  
  GType parent;
  GType type;

  type = GPOINTER_TO_INT (iter->user_data);

  parent = g_type_parent (type);

  if (parent == G_TYPE_INVALID)
    {
      /* fundamental type, add 1 */
      if ((type + 1) < G_TYPE_LAST_RESERVED_FUNDAMENTAL)
        {
          iter->user_data = GINT_TO_POINTER (type + 1);
          return TRUE;
        }
      else
        return FALSE;
    }
  else
    {
      GType* children = g_type_children (parent, NULL);
      gint i = 0;

      g_assert (children != NULL);
      
      while (children[i] != type)
        ++i;
  
      ++i;

      if (children[i] != G_TYPE_INVALID)
        {
          g_free (children);
          iter->user_data = GINT_TO_POINTER (children[i]);
          return TRUE;
        }
      else
        {
          g_free (children);
          return FALSE;
        }
    }
}

static gboolean
gtk_real_model_types_iter_children (GtkTreeModel *tree_model,
                                    GtkTreeIter  *iter,
                                    GtkTreeIter  *parent)
{
  GType type;
  GType* children;
  
  type = GPOINTER_TO_INT (parent->user_data);

  children = g_type_children (type, NULL);

  if (!children || children[0] == G_TYPE_INVALID)
    {
      g_free (children);
      return FALSE;
    }
  else
    {
      iter->user_data = GINT_TO_POINTER (children[0]);
      g_free (children);
      return TRUE;
    }
}

static gboolean
gtk_real_model_types_iter_has_child (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter)
{
  GType type;
  GType* children;
  
  type = GPOINTER_TO_INT (iter->user_data);
  
  children = g_type_children (type, NULL);

  if (!children || children[0] == G_TYPE_INVALID)
    {
      g_free (children);
      return FALSE;
    }
  else
    {
      g_free (children);
      return TRUE;
    }
}

static gint
gtk_real_model_types_iter_n_children (GtkTreeModel *tree_model,
                                      GtkTreeIter  *iter)
{
  if (iter == NULL)
    {
      return G_TYPE_LAST_RESERVED_FUNDAMENTAL - 1;
    }
  else
    {
      GType type;
      GType* children;
      guint n_children = 0;

      type = GPOINTER_TO_INT (iter->user_data);
      
      children = g_type_children (type, &n_children);
      
      g_free (children);
      
      return n_children;
    }
}

static gboolean
gtk_real_model_types_iter_nth_child (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter,
                                     GtkTreeIter  *parent,
                                     gint          n)
{  
  if (parent == NULL)
    {
      /* fundamental type */
      if (n < G_TYPE_LAST_RESERVED_FUNDAMENTAL)
        {
          iter->user_data = GINT_TO_POINTER (n);
          return TRUE;
        }
      else
        return FALSE;
    }
  else
    {
      GType type = GPOINTER_TO_INT (parent->user_data);      
      guint n_children = 0;
      GType* children = g_type_children (type, &n_children);

      if (n_children == 0)
        {
          g_free (children);
          return FALSE;
        }
      else if (n >= n_children)
        {
          g_free (children);
          return FALSE;
        }
      else
        {
          iter->user_data = GINT_TO_POINTER (children[n]);
          g_free (children);

          return TRUE;
        }
    }
}

static gboolean
gtk_real_model_types_iter_parent (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter,
                                  GtkTreeIter  *child)
{
  GType type;
  GType parent;
  
  type = GPOINTER_TO_INT (child->user_data);
  
  parent = g_type_parent (type);
  
  if (parent == G_TYPE_INVALID)
    {
      if (type >= G_TYPE_LAST_RESERVED_FUNDAMENTAL)
        g_warning ("no parent for %d %s\n", type, g_type_name (type));
      return FALSE;
    }
  else
    {
      iter->user_data = GINT_TO_POINTER (parent);
      
      return TRUE;
    }
}

/*
 * Property editor thingy
 */

static void
get_param_specs (GObject *object,
                 GParamSpec ***specs,
                 gint         *n_specs)
{
  /* Use private interface for now, fix later */
  *specs = G_OBJECT_GET_CLASS (object)->property_specs;
  *n_specs = G_OBJECT_GET_CLASS (object)->n_property_specs;
}

typedef struct
{
  gpointer instance;
  guint id;
} DisconnectData;

static void
disconnect_func (gpointer data)
{
  DisconnectData *dd = data;
  
  g_signal_handler_disconnect (dd->instance, dd->id);
  g_free (dd);
}

static void
g_object_connect_property (GObject *object,
                           const gchar *prop_name,
                           GtkSignalFunc func,
                           gpointer data,
                           GObject *alive_object)
{
  gchar *with_detail = g_strconcat ("notify::", prop_name, NULL);
  DisconnectData *dd;

  dd = g_new (DisconnectData, 1);
  
  dd->id = g_signal_connect_data (object, with_detail,
                                  func, data,
                                  NULL, FALSE, FALSE);

  dd->instance = object;
  
  g_object_set_data_full (G_OBJECT (alive_object),
                          "alive-object",
                          dd,
                          disconnect_func);
  
  g_free (with_detail);
}

typedef struct 
{
  GObject *obj;
  gchar *prop;
} ObjectProperty;

static void
free_object_property (ObjectProperty *p)
{
  g_free (p->prop);
  g_free (p);
}

static void
connect_controller (GObject *controller,
                    const gchar *signal,
                    GObject *model,
                    const gchar *prop_name,
                    GtkSignalFunc func)
{
  ObjectProperty *p;

  p = g_new (ObjectProperty, 1);
  p->obj = model;
  p->prop = g_strdup (prop_name);

  g_signal_connect_data (controller, signal, func, p,
                         (GClosureNotify)free_object_property,
                         FALSE, FALSE);
}

static void
int_modified (GtkAdjustment *adj, gpointer data)
{
  ObjectProperty *p = data;

  g_object_set (p->obj, p->prop, (int) adj->value, NULL);
}

static void
int_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = { 0, };  

  g_value_init (&val, G_TYPE_INT);
  g_object_get_property (object, pspec->name, &val);

  if (g_value_get_int (&val) != (int)adj->value)
    gtk_adjustment_set_value (adj, g_value_get_int (&val));

  g_value_unset (&val);
}

static void
float_modified (GtkAdjustment *adj, gpointer data)
{
  ObjectProperty *p = data;

  g_object_set (p->obj, p->prop, (float) adj->value, NULL);
}

static void
float_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = { 0, };  

  g_value_init (&val, G_TYPE_FLOAT);
  g_object_get_property (object, pspec->name, &val);

  if (g_value_get_float (&val) != (float) adj->value)
    gtk_adjustment_set_value (adj, g_value_get_float (&val));

  g_value_unset (&val);
}

static void
string_modified (GtkEntry *entry, gpointer data)
{
  ObjectProperty *p = data;
  gchar *text;

  text = gtk_entry_get_text (entry);

  g_object_set (p->obj, p->prop, text, NULL);
}

static void
string_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  GValue val = { 0, };  
  gchar *str;
  gchar *text;
  
  g_value_init (&val, G_TYPE_STRING);
  g_object_get_property (object, pspec->name, &val);

  str = g_value_get_string (&val);
  if (str == NULL)
    str = "";
  text = gtk_entry_get_text (entry);

  if (strcmp (str, text) != 0)
    gtk_entry_set_text (entry, str);
  
  g_value_unset (&val);
}

static void
bool_modified (GtkToggleButton *tb, gpointer data)
{
  ObjectProperty *p = data;

  g_object_set (p->obj, p->prop, (int) tb->active, NULL);
}

static void
bool_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkToggleButton *tb = GTK_TOGGLE_BUTTON (data);
  GValue val = { 0, };  
  
  g_value_init (&val, G_TYPE_BOOLEAN);
  g_object_get_property (object, pspec->name, &val);

  if (g_value_get_boolean (&val) != tb->active)
    gtk_toggle_button_set_active (tb, g_value_get_boolean (&val));

  gtk_label_set_text (GTK_LABEL (GTK_BIN (tb)->child), g_value_get_boolean (&val) ?
                      "TRUE" : "FALSE");
  
  g_value_unset (&val);
}


static void
enum_modified (GtkOptionMenu *om, gpointer data)
{
  ObjectProperty *p = data;
  gint i;
  GParamSpec *spec;
  GEnumClass *eclass;
  
  spec = g_object_class_find_property (G_OBJECT_GET_CLASS (p->obj),
                                       p->prop);

  eclass = G_ENUM_CLASS (g_type_class_peek (spec->value_type));
  
  i = gtk_option_menu_get_history (om);

  g_object_set (p->obj, p->prop, eclass->values[i].value, NULL);
}

static void
enum_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkOptionMenu *om = GTK_OPTION_MENU (data);
  GValue val = { 0, };  
  GEnumClass *eclass;
  gint i;

  eclass = G_ENUM_CLASS (g_type_class_peek (pspec->value_type));
  
  g_value_init (&val, pspec->value_type);
  g_object_get_property (object, pspec->name, &val);

  i = 0;
  while (i < eclass->n_values)
    {
      if (eclass->values[i].value == g_value_get_enum (&val))
        break;
      ++i;
    }
  
  if (gtk_option_menu_get_history (om) != i)
    gtk_option_menu_set_history (om, i);
  
  g_value_unset (&val);
}

static GtkWidget*
create_prop_editor (GObject *object)
{
  GtkWidget *win;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *prop_edit;
  GtkWidget *sw;
  gint n_specs = 0;
  GParamSpec **specs = NULL;
  gint i;
  GtkAdjustment *adj;
  
  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  /* hold a strong ref to the object we're editing */
  g_object_ref (G_OBJECT (object));
  g_object_set_data_full (G_OBJECT (win), "model-object",
                          object, (GDestroyNotify)g_object_unref);
  
  vbox = gtk_vbox_new (TRUE, 2);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), vbox);
  gtk_container_add (GTK_CONTAINER (win), sw);
  
  get_param_specs (object, &specs, &n_specs);
  
  i = 0;
  while (i < n_specs)
    {
      GParamSpec *spec = specs[i];
      gboolean can_modify;
      
      prop_edit = NULL;

      can_modify = ((spec->flags & G_PARAM_WRITABLE) != 0 &&
                    (spec->flags & G_PARAM_CONSTRUCT_ONLY) == 0);
      
      if ((spec->flags & G_PARAM_READABLE) == 0)
        {
          /* can't display unreadable properties */
          ++i;
          continue;
        }
      
      switch (spec->value_type)
        {
        case G_TYPE_INT:
          hbox = gtk_hbox_new (FALSE, 10);
          label = gtk_label_new (spec->nick);
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
          gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
          adj = GTK_ADJUSTMENT (gtk_adjustment_new (G_PARAM_SPEC_INT (spec)->default_value,
                                                    G_PARAM_SPEC_INT (spec)->minimum,
                                                    G_PARAM_SPEC_INT (spec)->maximum,
                                                    1,
                                                    MAX ((G_PARAM_SPEC_INT (spec)->maximum -
                                                          G_PARAM_SPEC_INT (spec)->minimum) / 10, 1),
                                                    0.0));

          prop_edit = gtk_spin_button_new (adj, 1.0, 0);
          gtk_box_pack_end (GTK_BOX (hbox), prop_edit, FALSE, FALSE, 0);
          
          gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 

          g_object_connect_property (object, spec->name,
                                     GTK_SIGNAL_FUNC (int_changed),
                                     adj, G_OBJECT (adj));

          if (can_modify)
            connect_controller (G_OBJECT (adj), "value_changed",
                                object, spec->name, (GtkSignalFunc) int_modified);
          break;

        case G_TYPE_FLOAT:
          hbox = gtk_hbox_new (FALSE, 10);
          label = gtk_label_new (spec->nick);
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
          gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
          adj = GTK_ADJUSTMENT (gtk_adjustment_new (G_PARAM_SPEC_FLOAT (spec)->default_value,
                                                    G_PARAM_SPEC_FLOAT (spec)->minimum,
                                                    G_PARAM_SPEC_FLOAT (spec)->maximum,
                                                    0.1,
                                                    MAX ((G_PARAM_SPEC_FLOAT (spec)->maximum -
                                                          G_PARAM_SPEC_FLOAT (spec)->minimum) / 10, 0.1),
                                                    0.0));

          prop_edit = gtk_spin_button_new (adj, 0.1, 2);
          
          gtk_box_pack_end (GTK_BOX (hbox), prop_edit, FALSE, FALSE, 0);
          
          gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 

          g_object_connect_property (object, spec->name,
                                     GTK_SIGNAL_FUNC (float_changed),
                                     adj, G_OBJECT (adj));

          if (can_modify)
            connect_controller (G_OBJECT (adj), "value_changed",
                                object, spec->name, (GtkSignalFunc) float_modified);
          break;
          
        case G_TYPE_STRING:
          hbox = gtk_hbox_new (FALSE, 10);
          label = gtk_label_new (spec->nick);
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
          gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

          prop_edit = gtk_entry_new ();
          gtk_box_pack_end (GTK_BOX (hbox), prop_edit, FALSE, FALSE, 0);
          
          gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 

          g_object_connect_property (object, spec->name,
                                     GTK_SIGNAL_FUNC (string_changed),
                                     prop_edit, G_OBJECT (prop_edit));

          if (can_modify)
            connect_controller (G_OBJECT (prop_edit), "changed",
                                object, spec->name, (GtkSignalFunc) string_modified);
          break;

        case G_TYPE_BOOLEAN:
          hbox = gtk_hbox_new (FALSE, 10);
          label = gtk_label_new (spec->nick);
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
          gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

          prop_edit = gtk_toggle_button_new_with_label ("");
          gtk_box_pack_end (GTK_BOX (hbox), prop_edit, FALSE, FALSE, 0);
          
          gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 

          g_object_connect_property (object, spec->name,
                                     GTK_SIGNAL_FUNC (bool_changed),
                                     prop_edit, G_OBJECT (prop_edit));

          if (can_modify)
            connect_controller (G_OBJECT (prop_edit), "toggled",
                                object, spec->name, (GtkSignalFunc) bool_modified);
          break;
          
        default:
          if (g_type_is_a (spec->value_type, G_TYPE_ENUM))
            {
              GtkWidget *menu;
              GEnumClass *eclass;
              gint i;
            
              hbox = gtk_hbox_new (FALSE, 10);
              label = gtk_label_new (spec->nick);
              gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
              gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
            
              prop_edit = gtk_option_menu_new ();

              menu = gtk_menu_new ();

              eclass = G_ENUM_CLASS (g_type_class_peek (spec->value_type));

              i = 0;
              while (i < eclass->n_values)
                {
                  GtkWidget *mi;
                
                  mi = gtk_menu_item_new_with_label (eclass->values[i].value_name);

                  gtk_widget_show (mi);
                
                  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
                
                  ++i;
                }

              gtk_option_menu_set_menu (GTK_OPTION_MENU (prop_edit), menu);
              
              gtk_box_pack_end (GTK_BOX (hbox), prop_edit, FALSE, FALSE, 0);
            
              gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 
            
              g_object_connect_property (object, spec->name,
                                         GTK_SIGNAL_FUNC (enum_changed),
                                         prop_edit, G_OBJECT (prop_edit));
            
              if (can_modify)
                connect_controller (G_OBJECT (prop_edit), "changed",
                                    object, spec->name, (GtkSignalFunc) enum_modified);
            }
          else
            {
              gchar *msg = g_strdup_printf ("%s: don't know how to edit type %s",
                                            spec->nick, g_type_name (spec->value_type));
              hbox = gtk_hbox_new (FALSE, 10);
              label = gtk_label_new (msg);            
              g_free (msg);
              gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
              gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
              gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
            }
          break;
        }

      if (prop_edit)
        {
          if (!can_modify)
            gtk_widget_set_sensitive (prop_edit, FALSE);
          
          /* set initial value */
          g_object_notify (object, spec->name);
        }
      
      ++i;
    }

  gtk_window_set_default_size (GTK_WINDOW (win), 300, 500);
  
  gtk_widget_show_all (win);

  return win;
}

/*
 * Automated testing
 */

static void
treestore_torture_recurse (GtkTreeStore *store,
                           GtkTreeIter  *root,
                           gint          depth)
{
  GtkTreeModel *model;
  gint i;
  GtkTreeIter iter;  
  
  model = GTK_TREE_MODEL (store);    

  if (depth > 2)
    return;

  ++depth;

  gtk_tree_store_append (store, &iter, root);
  
  gtk_tree_model_iter_children (model, &iter, root);
  
  i = 0;
  while (i < 100)
    {
      gtk_tree_store_append (store, &iter, root);
      ++i;
    }

  while (gtk_tree_model_iter_children (model, &iter, root))
    gtk_tree_store_remove (store, &iter);

  gtk_tree_store_append (store, &iter, root);

  /* inserts before last node in tree */
  i = 0;
  while (i < 100)
    {
      gtk_tree_store_insert_before (store, &iter, root, &iter);
      ++i;
    }

  /* inserts after the node before the last node */
  i = 0;
  while (i < 100)
    {
      gtk_tree_store_insert_after (store, &iter, root, &iter);
      ++i;
    }

  /* inserts after the last node */
  gtk_tree_store_append (store, &iter, root);
    
  i = 0;
  while (i < 100)
    {
      gtk_tree_store_insert_after (store, &iter, root, &iter);
      ++i;
    }

  /* remove everything again */
  while (gtk_tree_model_iter_children (model, &iter, root))
    gtk_tree_store_remove (store, &iter);


    /* Prepends */
  gtk_tree_store_prepend (store, &iter, root);
    
  i = 0;
  while (i < 100)
    {
      gtk_tree_store_prepend (store, &iter, root);
      ++i;
    }

  /* remove everything again */
  while (gtk_tree_model_iter_children (model, &iter, root))
    gtk_tree_store_remove (store, &iter);

  gtk_tree_store_append (store, &iter, root);
  gtk_tree_store_append (store, &iter, root);
  gtk_tree_store_append (store, &iter, root);
  gtk_tree_store_append (store, &iter, root);

  while (gtk_tree_model_iter_children (model, &iter, root))
    {
      treestore_torture_recurse (store, &iter, depth);
      gtk_tree_store_remove (store, &iter);
    }
}

static void
run_automated_tests (void)
{
  g_print ("Running automated tests...\n");
  
  /* FIXME TreePath basic verification */

  /* FIXME generic consistency checks on the models */

  {
    /* Make sure list store mutations don't crash anything */
    GtkListStore *store;
    GtkTreeModel *model;
    gint i;
    GtkTreeIter iter;
    
    store = gtk_list_store_new_with_types (1, G_TYPE_INT);

    model = GTK_TREE_MODEL (store);
    
    i = 0;
    while (i < 100)
      {
        gtk_list_store_append (store, &iter);
        ++i;
      }

    while (gtk_tree_model_get_first (model, &iter))
      gtk_list_store_remove (store, &iter);

    gtk_list_store_append (store, &iter);

    /* inserts before last node in list */
    i = 0;
    while (i < 100)
      {
        gtk_list_store_insert_before (store, &iter, &iter);
        ++i;
      }

    /* inserts after the node before the last node */
    i = 0;
    while (i < 100)
      {
        gtk_list_store_insert_after (store, &iter, &iter);
        ++i;
      }

    /* inserts after the last node */
    gtk_list_store_append (store, &iter);
    
    i = 0;
    while (i < 100)
      {
        gtk_list_store_insert_after (store, &iter, &iter);
        ++i;
      }

    /* remove everything again */
    while (gtk_tree_model_get_first (model, &iter))
      gtk_list_store_remove (store, &iter);


    /* Prepends */
    gtk_list_store_prepend (store, &iter);
    
    i = 0;
    while (i < 100)
      {
        gtk_list_store_prepend (store, &iter);
        ++i;
      }

    /* remove everything again */
    while (gtk_tree_model_get_first (model, &iter))
      gtk_list_store_remove (store, &iter);
    
    g_object_unref (G_OBJECT (store));
  }

  {
    /* Make sure tree store mutations don't crash anything */
    GtkTreeStore *store;
    
    store = gtk_tree_store_new_with_types (1, G_TYPE_INT);
    
    treestore_torture_recurse (store, NULL, 0);
    
    g_object_unref (G_OBJECT (store));
  }

  g_print ("Passed.\n");
}
