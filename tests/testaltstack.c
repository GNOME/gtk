#include <gtk/gtk.h>
#include "gtkdoubleselection.h"

static GtkSelectionModel *
get_model (void)
{
  GListStore *store = g_list_store_new (GTK_TYPE_WIDGET);

  g_list_store_append (store, gtk_label_new ("One"));
  g_list_store_append (store, gtk_label_new ("Two"));
  g_list_store_append (store, gtk_label_new ("Three"));
  g_list_store_append (store, gtk_label_new ("Four"));
  g_list_store_append (store, gtk_label_new ("Five"));

  return GTK_SELECTION_MODEL (gtk_double_selection_new (G_LIST_MODEL (store)));
}

static void
selection_changed_cb (GtkSelectionModel *model, guint position, guint n_items, gpointer data)
{
  guint i;
  PangoAttrList *attrs;
  PangoAttribute *attr;

  attrs = pango_attr_list_new ();
  attr = pango_attr_underline_new (PANGO_UNDERLINE_LOW);
  attr->start_index = 0;
  attr->end_index = -1;
  pango_attr_list_insert (attrs, attr);

  for (i = position; i < position + n_items; i++)
    {
      GtkWidget *child = g_list_model_get_item (G_LIST_MODEL (model), i);
      if (gtk_selection_model_is_selected (model, i))
        gtk_label_set_attributes (GTK_LABEL (child), attrs);
      else
        gtk_label_set_attributes (GTK_LABEL (child), NULL);
      g_object_unref (child);
    }

  pango_attr_list_unref (attrs);
}

static GtkWidget *
get_view (GtkSelectionModel *model)
{
  GtkWidget *box;
  guint i;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  g_object_set (box, "margin", 10, NULL);
  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (model)); i++)
    {
      GtkWidget *child = g_list_model_get_item (G_LIST_MODEL (model), i);
      gtk_container_add (GTK_CONTAINER (box), child);
      g_object_unref (child);
    }

  selection_changed_cb (model, 0, g_list_model_get_n_items (G_LIST_MODEL (model)), NULL);
  g_signal_connect (model, "selection-changed", G_CALLBACK (selection_changed_cb), NULL);
  return box;
}

static void
get_label_data (GObject *item,
                gpointer user_data,
                gboolean *visible,
                char **title,
                char **icon_name,
                gboolean *needs_attention)
{
  GtkWidget *label = GTK_WIDGET (item);
  *visible = gtk_widget_get_visible (label);
  *title = g_strdup (gtk_label_get_label (GTK_LABEL (label)));
  *icon_name = NULL;
  *needs_attention = FALSE;
}

int main (int argc, char *argv[])
{
  GtkWidget *window, *box, *switcher1, *switcher2, *view;
  GtkSelectionModel *model;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  switcher1 = gtk_stack_switcher_new ();
  switcher2 = gtk_stack_switcher_new ();

  model = get_model ();
  view = get_view (model);

  gtk_stack_switcher_set_model (GTK_STACK_SWITCHER (switcher1), model, get_label_data, NULL, NULL);
  gtk_stack_switcher_set_model (GTK_STACK_SWITCHER (switcher2), model, get_label_data, NULL, NULL);

  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_container_add (GTK_CONTAINER (box), switcher1);
  gtk_container_add (GTK_CONTAINER (box), view);
  gtk_container_add (GTK_CONTAINER (box), switcher2);

  gtk_widget_show (window);

  gtk_main ();

  return 0; 
}
