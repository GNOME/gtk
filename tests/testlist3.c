#include <gtk/gtk.h>

static const char *entries[] = {
  "GTK_LIST_BOX_ROW"
};

static void
drag_begin (GtkDragSource *source,
            GdkDrag       *drag,
            GtkWidget      *widget)
{
  GtkWidget *row;
  GtkAllocation alloc;
  GdkPaintable *paintable;
  int x, y;

  row = gtk_widget_get_ancestor (widget, GTK_TYPE_LIST_BOX_ROW);
  gtk_widget_get_allocation (row, &alloc);

  paintable = gtk_widget_paintable_new (row);
  gtk_widget_translate_coordinates (widget, row, 0, 0, &x, &y);
  gtk_drag_source_set_icon (source, paintable, -x, -y);

  g_object_unref (paintable);
}

static void
got_row (GObject      *src,
         GAsyncResult *result,
         gpointer      data)
{
  GtkDropTarget *dest = GTK_DROP_TARGET (src);
  GtkWidget *target = data;
  GtkWidget *row;
  GtkWidget *source;
  int pos;
  GtkSelectionData *selection_data;

  selection_data = gtk_drop_target_read_selection_finish (dest, result, NULL);

  pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (target));
  row = (gpointer)* (gpointer*)gtk_selection_data_get_data (selection_data);
  source = gtk_widget_get_ancestor (row, GTK_TYPE_LIST_BOX_ROW);

  gtk_selection_data_free (selection_data);

  if (source == target)
    return;

  g_object_ref (source);
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (source)), source);
  gtk_list_box_insert (GTK_LIST_BOX (gtk_widget_get_parent (target)), source, pos);
  g_object_unref (source);
}

static void
drag_drop (GtkDropTarget    *dest,
           GdkDrop          *drop,
           int               x,
           int               y,
           gpointer          data)
{
  gtk_drop_target_read_selection (dest, "GTK_LIST_BOX_ROW", NULL, got_row, data);
}

static GtkWidget *
create_row (const gchar *text)
{
  GtkWidget *row, *box, *label, *image;
  GBytes *bytes;
  GdkContentProvider *content;
  GdkContentFormats *targets;
  GtkDragSource *source;
  GtkDropTarget *dest;

  row = gtk_list_box_row_new (); 
  image = gtk_image_new_from_icon_name ("open-menu-symbolic");
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  g_object_set (box, "margin-start", 10, "margin-end", 10, NULL);
  label = gtk_label_new (text);
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_container_add (GTK_CONTAINER (box), image);

  bytes = g_bytes_new (&row, sizeof (gpointer));
  content = gdk_content_provider_new_for_bytes ("GTK_LIST_BOX_ROW", bytes);
  source = gtk_drag_source_new ();
  gtk_drag_source_set_content (source, content);
  gtk_drag_source_set_actions (source, GDK_ACTION_MOVE);
  g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), image);
  gtk_widget_add_controller (image, GTK_EVENT_CONTROLLER (source));

  targets = gdk_content_formats_new (entries, 1);
  dest = gtk_drop_target_new (targets, GDK_ACTION_MOVE);
  g_signal_connect (dest, "drag-drop", G_CALLBACK (drag_drop), row);
  gtk_widget_add_controller (GTK_WIDGET (row), GTK_EVENT_CONTROLLER (dest));

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

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
