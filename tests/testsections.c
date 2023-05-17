#include <gtk/gtk.h>

static void
setup_item (GtkSignalListItemFactory *self,
            GObject                  *object)
{
  GtkListItem *list_item = GTK_LIST_ITEM (object);
  GtkWidget *child = gtk_label_new ("");

  gtk_label_set_xalign (GTK_LABEL (child), 0);
  gtk_list_item_set_child (list_item, child);
}

static void
bind_item (GtkSignalListItemFactory *self,
           GObject                  *object)
{
  GtkListItem *list_item = GTK_LIST_ITEM (object);
  GObject *item = gtk_list_item_get_item (list_item);
  GtkWidget *child = gtk_list_item_get_child (list_item);

  gtk_label_set_label (GTK_LABEL (child),
                       gtk_string_object_get_string (GTK_STRING_OBJECT (item)));
}

static void
setup_header (GtkSignalListItemFactory *self,
              GObject                  *object)
{
  GtkListHeader *header = GTK_LIST_HEADER (object);
  GtkWidget *child = gtk_label_new ("");

  gtk_label_set_xalign (GTK_LABEL (child), 0);
  gtk_list_header_set_child (header, child);
}

static void
bind_header (GtkSignalListItemFactory *self,
             GObject                  *object)
{
  GtkListHeader *header = GTK_LIST_HEADER (object);
  GObject *item = gtk_list_header_get_item (header);
  GtkWidget *child = gtk_list_header_get_child (header);
  const char *string = gtk_string_object_get_string (GTK_STRING_OBJECT (item));
  char tmp[6] = { 0, };
  char *title;

  g_unichar_to_utf8 (g_utf8_get_char (string), tmp);

  title = g_strconcat ("<big><b>", tmp, "</b></big>", NULL);
  gtk_label_set_markup (GTK_LABEL (child), title);
  g_free (title);
}

static const char *strings[] = {
  "Alpha", "Andromeda", "Anaphylaxis", "Anaheim", "Beer", "Branch", "Botulism", "Banana",
  "Bee", "Crane", "Caldera", "Copper", "Crowd", "Dora", "Dolphin", "Dam", "Ding",
 NULL,
};

static char *
get_first (GObject *this)
{
  const char *s = gtk_string_object_get_string (GTK_STRING_OBJECT (this));

  return g_strndup (s, g_utf8_next_char (s) - s);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *sw;
  GtkWidget *lv;
  GtkWidget *gv;
  GtkWidget *header;
  GtkWidget *switcher;
  GtkWidget *stack;
  GtkListItemFactory *factory;
  GtkListItemFactory *header_factory;
  GtkExpression *expression;
  GtkSortListModel *sortmodel;
  GtkSelectionModel *selection;

  gtk_init ();

  window = gtk_window_new ();

  header = gtk_header_bar_new ();
  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  stack = gtk_stack_new ();
  gtk_window_set_child (GTK_WINDOW (window), stack);

  switcher = gtk_stack_switcher_new ();
  gtk_header_bar_set_title_widget (GTK_HEADER_BAR (header), switcher);

  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));

  expression = gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string");
  sortmodel = gtk_sort_list_model_new (G_LIST_MODEL (gtk_string_list_new (strings)),
                                       GTK_SORTER (gtk_string_sorter_new (expression)));
  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL, 0, NULL, (GCallback) get_first, NULL, NULL);
  gtk_sort_list_model_set_section_sorter (sortmodel, GTK_SORTER (gtk_string_sorter_new (expression)));
  selection = GTK_SELECTION_MODEL (gtk_no_selection_new (G_LIST_MODEL (sortmodel)));

  sw = gtk_scrolled_window_new ();
  gtk_stack_add_titled (GTK_STACK (stack), sw, "list", "List");

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);

  header_factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (header_factory, "setup", G_CALLBACK (setup_header), NULL);
  g_signal_connect (header_factory, "bind", G_CALLBACK (bind_header), NULL);

  lv = gtk_list_view_new (g_object_ref (selection), factory);
  gtk_list_view_set_header_factory (GTK_LIST_VIEW (lv), header_factory);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), lv);

  sw = gtk_scrolled_window_new ();
  gtk_stack_add_titled (GTK_STACK (stack), sw, "grid", "Grid");

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);

  header_factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (header_factory, "setup", G_CALLBACK (setup_header), NULL);
  g_signal_connect (header_factory, "bind", G_CALLBACK (bind_header), NULL);

  gv = gtk_grid_view_new (g_object_ref (selection), factory);
  gtk_grid_view_set_header_factory (GTK_GRID_VIEW (gv), header_factory);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), gv);

  gtk_grid_view_set_min_columns (GTK_GRID_VIEW (gv), 3);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, FALSE);

  g_object_unref (selection);

  return 0;
}
