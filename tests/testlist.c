#include <gtk/gtk.h>

#include <string.h>

typedef struct _Row Row;
typedef struct _RowClass RowClass;

struct _Row
{
  GtkListBoxRow parent_instance;
  GtkWidget *label;
  gint sort_id;
};

struct _RowClass
{
  GtkListBoxRowClass parent_class;
};

const char *css =
  "GtkListBoxRow {"
  " border-width: 1px;"
  " border-style: solid;"
  " border-color: blue;"
  "}"
  "GtkListBoxRow:prelight {"
  "background-color: green;"
  "}"
  "GtkListBoxRow:active {"
  "background-color: red;"
  "}";

G_DEFINE_TYPE (Row, row, GTK_TYPE_LIST_BOX_ROW)

static void
row_init (Row *row)
{
}

static void
row_class_init (RowClass *class)
{
}

GtkWidget *
row_new (const gchar* text, gint sort_id) {
  Row *row;

  row = g_object_new (row_get_type (), NULL);
  if (text != NULL)
    {
      row->label = gtk_label_new (text);
      gtk_container_add (GTK_CONTAINER (row), row->label);
      gtk_widget_show (row->label);
    }
  row->sort_id = sort_id;

  return GTK_WIDGET (row);
}


static void
update_separator_cb (Row *row, Row *before, gpointer data)
{
  GtkWidget *hbox, *l, *b;
  GList *children;

  if (before == NULL ||
      (row->label != NULL &&
       strcmp (gtk_label_get_text (GTK_LABEL (row->label)), "blah3") == 0))
    {
      /* Create separator if needed */
      if (gtk_list_box_row_get_separator (GTK_LIST_BOX_ROW (row)) == NULL)
        {
          hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
          l = gtk_label_new ("Separator");
          gtk_container_add (GTK_CONTAINER (hbox), l);
          b = gtk_button_new_with_label ("button");
          gtk_container_add (GTK_CONTAINER (hbox), b);
          gtk_widget_show (l);
          gtk_widget_show (b);
          gtk_list_box_row_set_separator (GTK_LIST_BOX_ROW (row), hbox);
      }

      hbox = gtk_list_box_row_get_separator(GTK_LIST_BOX_ROW (row));

      children = gtk_container_get_children (GTK_CONTAINER (hbox));
      l = children->data;
      g_list_free (children);
      gtk_label_set_text (GTK_LABEL (l), g_strdup_printf ("Separator %d", row->sort_id));
    }
  else
    {
      gtk_list_box_row_set_separator(GTK_LIST_BOX_ROW (row), NULL);
    }
}

static int
sort_cb (Row *a, Row *b, gpointer data)
{
  return a->sort_id - b->sort_id;
}

static int
reverse_sort_cb (Row *a, Row *b, gpointer data)
{
  return b->sort_id - a->sort_id;
}

static gboolean
filter_cb (Row *row, gpointer data)
{
  const char *text;

  if (row->label != NULL)
    {
      text = gtk_label_get_text (GTK_LABEL (row->label));
      return strcmp (text, "blah3") != 0;
    }

  return TRUE;
}

static void
row_activated_cb (GtkListBox *list_box,
                  GtkListBoxRow *row)
{
  g_print ("activated row %p\n", row);
}

static void
row_selected_cb (GtkListBox *list_box,
                 GtkListBoxRow *row)
{
  g_print ("selected row %p\n", row);
}

static void
sort_clicked_cb (GtkButton *button,
                 gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_sort_func (list, (GtkListBoxSortFunc)sort_cb, NULL, NULL);
}

static void
reverse_sort_clicked_cb (GtkButton *button,
                         gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_sort_func (list, (GtkListBoxSortFunc)reverse_sort_cb, NULL, NULL);
}

static void
filter_clicked_cb (GtkButton *button,
                   gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_filter_func (list, (GtkListBoxFilterFunc)filter_cb, NULL, NULL);
}

static void
unfilter_clicked_cb (GtkButton *button,
                   gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_filter_func (list, NULL, NULL, NULL);
}

static void
change_clicked_cb (GtkButton *button,
                   gpointer data)
{
  Row *row = data;

  if (strcmp (gtk_label_get_text (GTK_LABEL (row->label)), "blah3") == 0)
    {
      gtk_label_set_text (GTK_LABEL (row->label), "blah5");
      row->sort_id = 5;
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (row->label), "blah3");
      row->sort_id = 3;
    }
  gtk_list_box_row_changed (GTK_LIST_BOX_ROW (row));
}

static void
add_clicked_cb (GtkButton *button,
                gpointer data)
{
  GtkListBox *list = data;
  GtkWidget *new_row;
  static int new_button_nr = 1;

  new_row = row_new( g_strdup_printf ("blah2 new %d", new_button_nr), new_button_nr);
  gtk_widget_show_all (new_row);
  gtk_container_add (GTK_CONTAINER (list), new_row);
  new_button_nr++;
}

static void
separate_clicked_cb (GtkButton *button,
                     gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_separator_func (list, (GtkListBoxUpdateSeparatorFunc)update_separator_cb, NULL, NULL);
}

static void
unseparate_clicked_cb (GtkButton *button,
                       gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_separator_func (list, NULL, NULL, NULL);
}

static void
visibility_clicked_cb (GtkButton *button,
                       gpointer data)
{
  GtkWidget *row = data;

  gtk_widget_set_visible (row, !gtk_widget_get_visible (row));
}

static void
selection_mode_changed (GtkComboBox *combo, gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_selection_mode (list, gtk_combo_box_get_active (combo));
}

int
main (int argc, char *argv[])
{
  GtkCssProvider *provider;
  GtkWidget *window, *hbox, *vbox, *list, *row, *row3, *row_vbox, *row_hbox, *l;
  GtkWidget *check, *button, *combo, *scrolled;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (window), hbox);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1, NULL);
  gtk_style_context_add_provider_for_screen (gtk_widget_get_screen (window),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);


  list = gtk_list_box_new ();

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (hbox), vbox);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                  "GTK_SELECTION_NONE");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                  "GTK_SELECTION_SINGLE");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                  "GTK_SELECTION_BROWSE");
  g_signal_connect (combo, "changed", G_CALLBACK (selection_mode_changed), list);
  gtk_container_add (GTK_CONTAINER (vbox), combo);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), gtk_list_box_get_selection_mode (GTK_LIST_BOX (list)));

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scrolled), list);
  gtk_container_add (GTK_CONTAINER (hbox), scrolled);

  g_signal_connect (list, "row-activated", G_CALLBACK (row_activated_cb), NULL);
  g_signal_connect (list, "row-selected", G_CALLBACK (row_selected_cb), NULL);

  row = row_new ("blah4", 4);
  gtk_container_add (GTK_CONTAINER (list), row);
  row3 = row = row_new ("blah3", 3);
  gtk_container_add (GTK_CONTAINER (list), row);
  row = row_new ("blah1", 1);
  gtk_container_add (GTK_CONTAINER (list), row);
  row = row_new ("blah2", 2);
  gtk_container_add (GTK_CONTAINER (list), row);

  row = row_new (NULL, 0);
  row_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  row_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  l = gtk_label_new ("da box for da man");
  gtk_container_add (GTK_CONTAINER (row_hbox), l);
  check = gtk_check_button_new ();
  gtk_container_add (GTK_CONTAINER (row_hbox), check);
  button = gtk_button_new_with_label ("ya!");
  gtk_container_add (GTK_CONTAINER (row_hbox), button);
  gtk_container_add (GTK_CONTAINER (row_vbox), row_hbox);
  check = gtk_check_button_new ();
  gtk_container_add (GTK_CONTAINER (row_vbox), check);
  gtk_container_add (GTK_CONTAINER (row), row_vbox);
  gtk_container_add (GTK_CONTAINER (list), row);

  row = row_new (NULL, 0);
  button = gtk_button_new_with_label ("focusable row");
  gtk_widget_set_hexpand (button, FALSE);
  gtk_widget_set_halign (button, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (row), button);
  gtk_container_add (GTK_CONTAINER (list), row);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (hbox), vbox);

  button = gtk_button_new_with_label ("sort");
  gtk_container_add (GTK_CONTAINER (vbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (sort_clicked_cb), list);

  button = gtk_button_new_with_label ("reverse");
  gtk_container_add (GTK_CONTAINER (vbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (reverse_sort_clicked_cb), list);

  button = gtk_button_new_with_label ("change");
  gtk_container_add (GTK_CONTAINER (vbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (change_clicked_cb), row3);

  button = gtk_button_new_with_label ("filter");
  gtk_container_add (GTK_CONTAINER (vbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (filter_clicked_cb), list);

  button = gtk_button_new_with_label ("unfilter");
  gtk_container_add (GTK_CONTAINER (vbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (unfilter_clicked_cb), list);

  button = gtk_button_new_with_label ("add");
  gtk_container_add (GTK_CONTAINER (vbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (add_clicked_cb), list);

  button = gtk_button_new_with_label ("separate");
  gtk_container_add (GTK_CONTAINER (vbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (separate_clicked_cb), list);

  button = gtk_button_new_with_label ("unseparate");
  gtk_container_add (GTK_CONTAINER (vbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (unseparate_clicked_cb), list);

  button = gtk_button_new_with_label ("visibility");
  gtk_container_add (GTK_CONTAINER (vbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (visibility_clicked_cb), row3);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
