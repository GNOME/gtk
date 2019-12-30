#include <gtk/gtk.h>

static const char *entries[] = {
  "GTK_LIST_BOX_ROW"
};

static void
drag_begin (GtkWidget      *widget,
            GdkDrag        *drag,
            gpointer        data)
{
  GtkWidget *row;
  GtkAllocation alloc;
  GdkPaintable *paintable;
  int x, y;

  row = gtk_widget_get_ancestor (widget, GTK_TYPE_LIST_BOX_ROW);
  gtk_widget_get_allocation (row, &alloc);

  paintable = gtk_widget_paintable_new (row);
  gtk_widget_translate_coordinates (widget, row, 0, 0, &x, &y);
  gtk_drag_set_icon_paintable (drag, paintable, -x, -y);

  g_object_unref (paintable);
}


void
drag_data_get (GtkWidget        *widget,
               GdkDrag          *drag,
               GtkSelectionData *selection_data,
               gpointer          data)
{
  gtk_selection_data_set (selection_data,
                          g_intern_static_string ("GTK_LIST_BOX_ROW"),
                          32,
                          (const guchar *)&widget,
                          sizeof (gpointer));
}


static void
drag_data_received (GtkWidget        *widget,
                    GdkDrop          *drop,
                    GtkSelectionData *selection_data,
                    gpointer          data)
{
  GtkWidget *target;
  GtkWidget *row;
  GtkWidget *source;
  int pos;

  target = widget;

  pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (target));
  row = (gpointer)* (gpointer*)gtk_selection_data_get_data (selection_data);
  source = gtk_widget_get_ancestor (row, GTK_TYPE_LIST_BOX_ROW);

  if (source == target)
    return;

  g_object_ref (source);
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (source)), source);
  gtk_list_box_insert (GTK_LIST_BOX (gtk_widget_get_parent (target)), source, pos);
  g_object_unref (source);
}

static GtkWidget *
create_row (const gchar *text)
{
  GtkWidget *row, *box, *label, *image;
  GdkContentFormats *targets;

  row = gtk_list_box_row_new (); 
  image = gtk_image_new_from_icon_name ("open-menu-symbolic");
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  g_object_set (box, "margin-start", 10, "margin-end", 10, NULL);
  label = gtk_label_new (text);
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_container_add (GTK_CONTAINER (box), image);

  targets = gdk_content_formats_new (entries, 1);

  gtk_drag_source_set (image, GDK_BUTTON1_MASK, targets, GDK_ACTION_MOVE);
  g_signal_connect (image, "drag-begin", G_CALLBACK (drag_begin), NULL);
  g_signal_connect (image, "drag-data-get", G_CALLBACK (drag_data_get), NULL);

  gtk_drag_dest_set (row, GTK_DEST_DEFAULT_ALL, targets, GDK_ACTION_MOVE);
  g_signal_connect (row, "drag-data-received", G_CALLBACK (drag_data_received), NULL);

  gdk_content_formats_unref (targets);

  return row;
}

static void
on_row_activated (GtkListBox *self,
                  GtkWidget  *child)
{
  const char *id;
  id = g_object_get_data (G_OBJECT (gtk_bin_get_child (GTK_BIN (child))), "id");
  g_message ("Row activated %p: %s", child, id);
}

static void
on_selected_children_changed (GtkListBox *self)
{
  g_message ("Selection changed");
}

static void
a11y_selection_changed (AtkObject *obj)
{
  g_message ("Accessible selection changed");
}

static void
selection_mode_changed (GtkComboBox *combo, gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_selection_mode (list, gtk_combo_box_get_active (combo));
}

static const char *css =
  ".during-dnd { "
  "  background: white; "
  "  border: 1px solid black; "
  "}";

int
main (int argc, char *argv[])
{
  GtkWidget *window, *list, *sw, *row;
  GtkWidget *hbox, *vbox, *combo, *button;
  gint i;
  gchar *text;
  GtkCssProvider *provider;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (provider), 800);
  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), -1, 300);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_add (GTK_CONTAINER (window), hbox);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  g_object_set (vbox, "margin", 12, NULL);
  gtk_container_add (GTK_CONTAINER (hbox), vbox);

  list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);

  g_signal_connect (list, "row-activated", G_CALLBACK (on_row_activated), NULL);
  g_signal_connect (list, "selected-rows-changed", G_CALLBACK (on_selected_children_changed), NULL);
  g_signal_connect (gtk_widget_get_accessible (list), "selection-changed", G_CALLBACK (a11y_selection_changed), NULL);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_container_add (GTK_CONTAINER (hbox), sw);
  gtk_container_add (GTK_CONTAINER (sw), list);

  button = gtk_check_button_new_with_label ("Activate on single click");
  g_object_bind_property (list, "activate-on-single-click",
                          button, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (vbox), button);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "None");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Single");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Browse");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Multiple");
  g_signal_connect (combo, "changed", G_CALLBACK (selection_mode_changed), list);
  gtk_container_add (GTK_CONTAINER (vbox), combo);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), gtk_list_box_get_selection_mode (GTK_LIST_BOX (list)));

  for (i = 0; i < 20; i++)
    {
      text = g_strdup_printf ("Row %d", i);
      row = create_row (text);
      gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);
    }

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
