#include <gtk/gtk.h>

GtkWidget *stack;
GtkWidget *switcher;
GtkWidget *sidebar;
GtkWidget *w1;

static void
set_visible_child (GtkWidget *button, gpointer data)
{
  gtk_stack_set_visible_child (GTK_STACK (stack), GTK_WIDGET (data));
}

static void
set_visible_child_name (GtkWidget *button, gpointer data)
{
  gtk_stack_set_visible_child_name (GTK_STACK (stack), (const char *)data);
}

static void
toggle_homogeneous (GtkWidget *button, gpointer data)
{
  gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
  gtk_stack_set_homogeneous (GTK_STACK (stack), active);
}

static void
toggle_icon_name (GtkWidget *button, gpointer data)
{
  gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
  gtk_container_child_set (GTK_CONTAINER (stack), w1,
			   "icon-name", active ? "edit-find-symbolic" : NULL,
			   NULL);
}

static void
toggle_transitions (GtkWidget *combo, gpointer data)
{
  int id = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
  gtk_stack_set_transition_type (GTK_STACK (stack), id);
}

static void
on_back_button_clicked (GtkButton *button, GtkStack *stack)
{
  const gchar *seq[] = { "1", "2", "3" };
  const gchar *vis;
  gint i;

  vis = gtk_stack_get_visible_child_name (stack);

  for (i = 1; i < G_N_ELEMENTS (seq); i++)
    {
      if (g_strcmp0 (vis, seq[i]) == 0)
        {
          gtk_stack_set_visible_child_full (stack, seq[i - 1], GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
          break;
        }
    }
}

static void
on_forward_button_clicked (GtkButton *button, GtkStack *stack)
{
  const gchar *seq[] = { "1", "2", "3" };
  const gchar *vis;
  gint i;

  vis = gtk_stack_get_visible_child_name (stack);

  for (i = 0; i < G_N_ELEMENTS (seq) - 1; i++)
    {
      if (g_strcmp0 (vis, seq[i]) == 0)
        {
          gtk_stack_set_visible_child_full (stack, seq[i + 1], GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
          break;
        }
    }
}

static void
update_back_button_sensitivity (GtkStack *stack, GParamSpec *pspec, GtkWidget *button)
{
  const gchar *vis;

  vis = gtk_stack_get_visible_child_name (stack);
  gtk_widget_set_sensitive (button, g_strcmp0 (vis, "1") != 0);
}

static void
update_forward_button_sensitivity (GtkStack *stack, GParamSpec *pspec, GtkWidget *button)
{
  const gchar *vis;

  vis = gtk_stack_get_visible_child_name (stack);
  gtk_widget_set_sensitive (button, g_strcmp0 (vis, "3") != 0);
}

gint
main (gint argc,
      gchar ** argv)
{
  GtkWidget *window, *box, *button, *hbox, *combo, *layout;
  GtkWidget *w2, *w3;
  GtkListStore* store;
  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkWidget *scrolled_win;
  int i;
  GtkTreeIter iter;
  GEnumClass *class;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, 300, 300);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box);

  switcher = gtk_stack_switcher_new ();
  gtk_box_pack_start (GTK_BOX (box), switcher, FALSE, FALSE, 0);

  stack = gtk_stack_new ();

  /* Make transitions longer so we can see that they work */
  gtk_stack_set_transition_duration (GTK_STACK (stack), 1500);

  gtk_widget_set_halign (stack, GTK_ALIGN_START);

  /* Add sidebar before stack */
  sidebar = gtk_sidebar_new ();
  gtk_sidebar_set_stack (GTK_SIDEBAR (sidebar), GTK_STACK (stack));
  layout = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (layout), sidebar, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (layout), stack, TRUE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (box), layout);

  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));

  w1 = gtk_text_view_new ();
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (w1)),
			    "This is a\nTest\nBalh!", -1);

  gtk_container_add_with_properties (GTK_CONTAINER (stack), w1,
				     "name", "1",
				     "title", "1",
				     NULL);

  w2 = gtk_button_new_with_label ("Gazoooooooooooooooonk");
  gtk_container_add (GTK_CONTAINER (stack), w2);
  gtk_container_child_set (GTK_CONTAINER (stack), w2,
			   "name", "2",
			   "title", "2",
                           "needs-attention", TRUE,
			   NULL);


  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_widget_set_size_request (scrolled_win, 100, 200);


  store = gtk_list_store_new (1, G_TYPE_STRING);

  for (i = 0; i < 40; i++)
    gtk_list_store_insert_with_values (store, &iter, i, 0,  "Testvalule", -1);

  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));

  gtk_container_add (GTK_CONTAINER (scrolled_win), tree_view);
  w3 = scrolled_win;

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Target", renderer,
						     "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  gtk_stack_add_titled (GTK_STACK (stack), w3, "3", "3");

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (box), hbox);

  button = gtk_button_new_with_label ("1");
  gtk_container_add (GTK_CONTAINER (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) set_visible_child, w1);

  button = gtk_button_new_with_label ("2");
  gtk_container_add (GTK_CONTAINER (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) set_visible_child, w2);

  button = gtk_button_new_with_label ("3");
  gtk_container_add (GTK_CONTAINER (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) set_visible_child, w3);

  button = gtk_button_new_with_label ("1");
  gtk_container_add (GTK_CONTAINER (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) set_visible_child_name, (gpointer) "1");

  button = gtk_button_new_with_label ("2");
  gtk_container_add (GTK_CONTAINER (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) set_visible_child_name, (gpointer) "2");

  button = gtk_button_new_with_label ("3");
  gtk_container_add (GTK_CONTAINER (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) set_visible_child_name, (gpointer) "3");

  button = gtk_check_button_new_with_label ("homogeneous");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				gtk_stack_get_homogeneous (GTK_STACK (stack)));
  gtk_container_add (GTK_CONTAINER (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) toggle_homogeneous, NULL);

  button = gtk_toggle_button_new_with_label ("Add icon");
  g_signal_connect (button, "toggled", (GCallback) toggle_icon_name, NULL);
  gtk_container_add (GTK_CONTAINER (hbox), button);

  combo = gtk_combo_box_text_new ();
  class = g_type_class_ref (GTK_TYPE_STACK_TRANSITION_TYPE);
  for (i = 0; i < class->n_values; i++)
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), class->values[i].value_nick);
  g_type_class_unref (class);

  gtk_container_add (GTK_CONTAINER (hbox), combo);
  g_signal_connect (combo, "changed", (GCallback) toggle_transitions, NULL);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (box), hbox);

  button = gtk_button_new_with_label ("<");
  g_signal_connect (button, "clicked", (GCallback) on_back_button_clicked, stack);
  g_signal_connect (stack, "notify::visible-child-name",
                    (GCallback)update_back_button_sensitivity, button);
  gtk_container_add (GTK_CONTAINER (hbox), button);

  button = gtk_button_new_with_label (">");
  gtk_container_add (GTK_CONTAINER (hbox), button);
  g_signal_connect (button, "clicked", (GCallback) on_forward_button_clicked, stack);
  g_signal_connect (stack, "notify::visible-child-name",
                    (GCallback)update_forward_button_sensitivity, button);


  gtk_widget_show_all (window);
  gtk_main ();

  gtk_widget_destroy (window);

  return 0;
}
