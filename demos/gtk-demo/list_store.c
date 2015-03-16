/* Tree View/List Store
 *
 * The GtkListStore is used to store data in list form, to be used
 * later on by a GtkTreeView to display it. This demo builds a
 * simple GtkListStore and displays it.
 *
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;
static GtkTreeModel *model = NULL;
static guint timeout = 0;

typedef struct
{
  const gboolean  fixed;
  const guint     number;
  const gchar    *severity;
  const gchar    *description;
}
Bug;

enum
{
  COLUMN_FIXED,
  COLUMN_NUMBER,
  COLUMN_SEVERITY,
  COLUMN_DESCRIPTION,
  COLUMN_PULSE,
  COLUMN_ICON,
  COLUMN_ACTIVE,
  COLUMN_SENSITIVE,
  NUM_COLUMNS
};

static Bug data[] =
{
  { FALSE, 60482, "Normal",     "scrollable notebooks and hidden tabs" },
  { FALSE, 60620, "Critical",   "gdk_window_clear_area (gdkwindow-win32.c) is not thread-safe" },
  { FALSE, 50214, "Major",      "Xft support does not clean up correctly" },
  { TRUE,  52877, "Major",      "GtkFileSelection needs a refresh method. " },
  { FALSE, 56070, "Normal",     "Can't click button after setting in sensitive" },
  { TRUE,  56355, "Normal",     "GtkLabel - Not all changes propagate correctly" },
  { FALSE, 50055, "Normal",     "Rework width/height computations for TreeView" },
  { FALSE, 58278, "Normal",     "gtk_dialog_set_response_sensitive () doesn't work" },
  { FALSE, 55767, "Normal",     "Getters for all setters" },
  { FALSE, 56925, "Normal",     "Gtkcalender size" },
  { FALSE, 56221, "Normal",     "Selectable label needs right-click copy menu" },
  { TRUE,  50939, "Normal",     "Add shift clicking to GtkTextView" },
  { FALSE, 6112,  "Enhancement","netscape-like collapsable toolbars" },
  { FALSE, 1,     "Normal",     "First bug :=)" },
};

static gboolean
spinner_timeout (gpointer data)
{
  GtkTreeIter iter;
  guint pulse;

  if (model == NULL)
    return G_SOURCE_REMOVE;

  gtk_tree_model_get_iter_first (model, &iter);
  gtk_tree_model_get (model, &iter,
                      COLUMN_PULSE, &pulse,
                      -1);
  if (pulse == G_MAXUINT)
    pulse = 0;
  else
    pulse++;

  gtk_list_store_set (GTK_LIST_STORE (model),
                      &iter,
                      COLUMN_PULSE, pulse,
                      COLUMN_ACTIVE, TRUE,
                      -1);

  return G_SOURCE_CONTINUE;
}

static GtkTreeModel *
create_model (void)
{
  gint i = 0;
  GtkListStore *store;
  GtkTreeIter iter;

  /* create list store */
  store = gtk_list_store_new (NUM_COLUMNS,
                              G_TYPE_BOOLEAN,
                              G_TYPE_UINT,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_UINT,
                              G_TYPE_STRING,
                              G_TYPE_BOOLEAN,
                              G_TYPE_BOOLEAN);

  /* add data to the list store */
  for (i = 0; i < G_N_ELEMENTS (data); i++)
    {
      gchar *icon_name;
      gboolean sensitive;

      if (i == 1 || i == 3)
        icon_name = "battery-caution-charging-symbolic";
      else
        icon_name = NULL;
      if (i == 3)
        sensitive = FALSE;
      else
        sensitive = TRUE;
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          COLUMN_FIXED, data[i].fixed,
                          COLUMN_NUMBER, data[i].number,
                          COLUMN_SEVERITY, data[i].severity,
                          COLUMN_DESCRIPTION, data[i].description,
                          COLUMN_PULSE, 0,
                          COLUMN_ICON, icon_name,
                          COLUMN_ACTIVE, FALSE,
                          COLUMN_SENSITIVE, sensitive,
                          -1);
    }

  return GTK_TREE_MODEL (store);
}

static void
fixed_toggled (GtkCellRendererToggle *cell,
               gchar                 *path_str,
               gpointer               data)
{
  GtkTreeModel *model = (GtkTreeModel *)data;
  GtkTreeIter  iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  gboolean fixed;

  /* get toggled iter */
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, COLUMN_FIXED, &fixed, -1);

  /* do something with the value */
  fixed ^= 1;

  /* set new value */
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, COLUMN_FIXED, fixed, -1);

  /* clean up */
  gtk_tree_path_free (path);
}

static void
add_columns (GtkTreeView *treeview)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);

  /* column for fixed toggles */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled",
                    G_CALLBACK (fixed_toggled), model);

  column = gtk_tree_view_column_new_with_attributes ("Fixed?",
                                                     renderer,
                                                     "active", COLUMN_FIXED,
                                                     NULL);

  /* set this column to a fixed sizing (of 50 pixels) */
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),
                                   GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (column), 50);
  gtk_tree_view_append_column (treeview, column);

  /* column for bug numbers */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Bug number",
                                                     renderer,
                                                     "text",
                                                     COLUMN_NUMBER,
                                                     NULL);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_NUMBER);
  gtk_tree_view_append_column (treeview, column);

  /* column for severities */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Severity",
                                                     renderer,
                                                     "text",
                                                     COLUMN_SEVERITY,
                                                     NULL);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_SEVERITY);
  gtk_tree_view_append_column (treeview, column);

  /* column for description */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Description",
                                                     renderer,
                                                     "text",
                                                     COLUMN_DESCRIPTION,
                                                     NULL);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_DESCRIPTION);
  gtk_tree_view_append_column (treeview, column);

  /* column for spinner */
  renderer = gtk_cell_renderer_spinner_new ();
  column = gtk_tree_view_column_new_with_attributes ("Spinning",
                                                     renderer,
                                                     "pulse",
                                                     COLUMN_PULSE,
                                                     "active",
                                                     COLUMN_ACTIVE,
                                                     NULL);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_PULSE);
  gtk_tree_view_append_column (treeview, column);

  /* column for symbolic icon */
  renderer = gtk_cell_renderer_pixbuf_new ();
  column = gtk_tree_view_column_new_with_attributes ("Symbolic icon",
                                                     renderer,
                                                     "icon-name",
                                                     COLUMN_ICON,
                                                     "sensitive",
                                                     COLUMN_SENSITIVE,
                                                     NULL);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_ICON);
  gtk_tree_view_append_column (treeview, column);
}

static gboolean
window_closed (GtkWidget *widget,
               GdkEvent  *event,
               gpointer   user_data)
{
  model = NULL;
  window = NULL;
  if (timeout != 0)
    {
      g_source_remove (timeout);
      timeout = 0;
    }
  return FALSE;
}

GtkWidget *
do_list_store (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *vbox;
      GtkWidget *label;
      GtkWidget *sw;
      GtkWidget *treeview;

      /* create window, etc */
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "GtkListStore demo");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      gtk_container_set_border_width (GTK_CONTAINER (window), 8);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      label = gtk_label_new ("This is the bug list (note: not based on real data, it would be nice to have a nice ODBC interface to bugzilla or so, though).");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                           GTK_SHADOW_ETCHED_IN);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);

      /* create tree model */
      model = create_model ();

      /* create tree view */
      treeview = gtk_tree_view_new_with_model (model);
      gtk_tree_view_set_search_column (GTK_TREE_VIEW (treeview),
                                       COLUMN_DESCRIPTION);

      g_object_unref (model);

      gtk_container_add (GTK_CONTAINER (sw), treeview);

      /* add columns to the tree view */
      add_columns (GTK_TREE_VIEW (treeview));

      /* finish & show */
      gtk_window_set_default_size (GTK_WINDOW (window), 280, 250);
      g_signal_connect (window, "delete-event",
                        G_CALLBACK (window_closed), NULL);
    }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_show_all (window);
      if (timeout == 0) {
        /* FIXME this should use the animation-duration instead */
        timeout = g_timeout_add (80, spinner_timeout, NULL);
      }
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
      if (timeout != 0)
        {
          g_source_remove (timeout);
          timeout = 0;
        }
    }

  return window;
}
