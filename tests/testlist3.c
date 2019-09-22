#include <gtk/gtk.h>

static GtkTargetEntry entries[] = {
  { "GTK_LIST_BOX_ROW", GTK_TARGET_SAME_APP, 0 }
};

static void
drag_begin (GtkWidget      *widget,
            GdkDragContext *context,
            gpointer        data)
{
  GtkWidget *row;
  GtkAllocation alloc;
  cairo_surface_t *surface;
  cairo_t *cr;
  int x, y;
  double sx, sy;

  row = gtk_widget_get_ancestor (widget, GTK_TYPE_LIST_BOX_ROW);
  gtk_widget_get_allocation (row, &alloc);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, alloc.width, alloc.height);
  cr = cairo_create (surface);

  gtk_style_context_add_class (gtk_widget_get_style_context (row), "drag-icon");
  gtk_widget_draw (row, cr);
  gtk_style_context_remove_class (gtk_widget_get_style_context (row), "drag-icon");

  gtk_widget_translate_coordinates (widget, row, 0, 0, &x, &y);
  cairo_surface_get_device_scale (surface, &sx, &sy);
  cairo_surface_set_device_offset (surface, -x * sy, -y * sy);
  gtk_drag_set_icon_surface (context, surface);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  g_object_set_data (G_OBJECT (gtk_widget_get_parent (row)), "drag-row", row);
  gtk_style_context_add_class (gtk_widget_get_style_context (row), "drag-row");
}

static void
drag_end (GtkWidget      *widget,
          GdkDragContext *context,
          gpointer        data)
{
  GtkWidget *row;

  row = gtk_widget_get_ancestor (widget, GTK_TYPE_LIST_BOX_ROW);
  g_object_set_data (G_OBJECT (gtk_widget_get_parent (row)), "drag-row", NULL);
  gtk_style_context_remove_class (gtk_widget_get_style_context (row), "drag-row");
  gtk_style_context_remove_class (gtk_widget_get_style_context (row), "drag-hover");
}

void
drag_data_get (GtkWidget        *widget,
               GdkDragContext   *context,
               GtkSelectionData *selection_data,
               guint             info,
               guint             time,
               gpointer          data)
{
  gtk_selection_data_set (selection_data,
                          gdk_atom_intern_static_string ("GTK_LIST_BOX_ROW"),
                          32,
                          (const guchar *)&widget,
                          sizeof (gpointer));
}

static GtkListBoxRow *
get_last_row (GtkListBox *list)
{
  int i;
  GtkListBoxRow *row;

  row = NULL;
  for (i = 0; ; i++)
    {
      GtkListBoxRow *tmp;
      tmp = gtk_list_box_get_row_at_index (list, i);
      if (tmp == NULL)
        return row;
      row = tmp;
    }
  return row;
}

static GtkListBoxRow *
get_row_before (GtkListBox    *list,
                GtkListBoxRow *row)
{
  int pos = gtk_list_box_row_get_index (row);
  return gtk_list_box_get_row_at_index (list, pos - 1);
}

static GtkListBoxRow *
get_row_after (GtkListBox    *list,
               GtkListBoxRow *row)
{
  int pos = gtk_list_box_row_get_index (row);
  return gtk_list_box_get_row_at_index (list, pos + 1);
}

static void
drag_data_received (GtkWidget        *widget,
                    GdkDragContext   *context,
                    gint              x,
                    gint              y,
                    GtkSelectionData *selection_data,
                    guint             info,
                    guint32           time,
                    gpointer          data)
{
  GtkWidget *row_before;
  GtkWidget *row_after;
  GtkWidget *row;
  GtkWidget *source;
  int pos;

  row_before = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "row-before"));
  row_after = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "row-after"));

  g_object_set_data (G_OBJECT (widget), "row-before", NULL);
  g_object_set_data (G_OBJECT (widget), "row-after", NULL);

  if (row_before)
    gtk_style_context_remove_class (gtk_widget_get_style_context (row_before), "drag-hover-bottom");
  if (row_after)
    gtk_style_context_remove_class (gtk_widget_get_style_context (row_after), "drag-hover-top");

  row = (gpointer)* (gpointer*)gtk_selection_data_get_data (selection_data);
  source = gtk_widget_get_ancestor (row, GTK_TYPE_LIST_BOX_ROW);

  if (source == row_after)
    return;

  g_object_ref (source);
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (source)), source);

  if (row_after)
    pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row_after));
  else
    pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row_before)) + 1;

  gtk_list_box_insert (GTK_LIST_BOX (widget), source, pos);
  g_object_unref (source);
}

static gboolean
drag_motion (GtkWidget      *widget,
             GdkDragContext *context,
             int             x,
             int             y,
             guint           time)
{
  GtkAllocation alloc;
  GtkWidget *row;
  int hover_row_y;
  int hover_row_height;
  GtkWidget *drag_row;
  GtkWidget *row_before;
  GtkWidget *row_after;

  row = GTK_WIDGET (gtk_list_box_get_row_at_y (GTK_LIST_BOX (widget), y));

  drag_row = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "drag-row"));
  row_before = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "row-before"));
  row_after = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "row-after"));

  gtk_style_context_remove_class (gtk_widget_get_style_context (drag_row), "drag-hover");
  if (row_before)
    gtk_style_context_remove_class (gtk_widget_get_style_context (row_before), "drag-hover-bottom");
  if (row_after)
    gtk_style_context_remove_class (gtk_widget_get_style_context (row_after), "drag-hover-top");

  if (row)
    {
      gtk_widget_get_allocation (row, &alloc);
      hover_row_y = alloc.y;
      hover_row_height = alloc.height;

      if (y < hover_row_y + hover_row_height/2)
        {
          row_after = row;
          row_before = GTK_WIDGET (get_row_before (GTK_LIST_BOX (widget), GTK_LIST_BOX_ROW (row)));
        }
      else
        {
          row_before = row;
          row_after = GTK_WIDGET (get_row_after (GTK_LIST_BOX (widget), GTK_LIST_BOX_ROW (row)));
        }
    }
  else
    {
      row_before = GTK_WIDGET (get_last_row (GTK_LIST_BOX (widget)));
      row_after = NULL;
    }

  g_object_set_data (G_OBJECT (widget), "row-before", row_before);
  g_object_set_data (G_OBJECT (widget), "row-after", row_after);

  if (drag_row == row_before || drag_row == row_after)
    {
      gtk_style_context_add_class (gtk_widget_get_style_context (drag_row), "drag-hover");
      return FALSE;
    }

  if (row_before)
    gtk_style_context_add_class (gtk_widget_get_style_context (row_before), "drag-hover-bottom");
  if (row_after)
    gtk_style_context_add_class (gtk_widget_get_style_context (row_after), "drag-hover-top");

  return TRUE;
}

static void
drag_leave (GtkWidget      *widget,
            GdkDragContext *context,
            guint           time)
{
  GtkWidget *drag_row;
  GtkWidget *row_before;
  GtkWidget *row_after;

  drag_row = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "drag-row"));
  row_before = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "row-before"));
  row_after = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "row-after"));

  gtk_style_context_remove_class (gtk_widget_get_style_context (drag_row), "drag-hover");
  if (row_before)
    gtk_style_context_remove_class (gtk_widget_get_style_context (row_before), "drag-hover-bottom");
  if (row_after)
    gtk_style_context_remove_class (gtk_widget_get_style_context (row_after), "drag-hover-top");
}

static GtkWidget *
create_row (const gchar *text)
{
  GtkWidget *row, *ebox, *box, *label, *image;

  row = gtk_list_box_row_new ();
  ebox = gtk_event_box_new ();
  image = gtk_image_new_from_icon_name ("open-menu-symbolic", 1);
  gtk_container_add (GTK_CONTAINER (ebox), image);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  g_object_set (box, "margin-start", 10, "margin-end", 10, NULL);
  label = gtk_label_new (text);
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (box), ebox);

  gtk_style_context_add_class (gtk_widget_get_style_context (row), "row");
  gtk_drag_source_set (ebox, GDK_BUTTON1_MASK, entries, 1, GDK_ACTION_MOVE);
  g_signal_connect (ebox, "drag-begin", G_CALLBACK (drag_begin), NULL);
  g_signal_connect (ebox, "drag-end", G_CALLBACK (drag_end), NULL);
  g_signal_connect (ebox, "drag-data-get", G_CALLBACK (drag_data_get), NULL);

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
  ".row:not(:first-child) { "
  "  border-top: 1px solid alpha(gray,0.5); "
  "  border-bottom: 1px solid transparent; "
  "}"
  ".row:first-child { "
  "  border-top: 1px solid transparent; "
  "  border-bottom: 1px solid transparent; "
  "}"
  ".row:last-child { "
  "  border-top: 1px solid alpha(gray,0.5); "
  "  border-bottom: 1px solid alpha(gray,0.5); "
  "}"
  ".row.drag-icon { "
  "  background: white; "
  "  border: 1px solid black; "
  "}"
  ".row.drag-row { "
  "  color: gray; "
  "  background: alpha(gray,0.2); "
  "}"
  ".row.drag-row.drag-hover { "
  "  border-top: 1px solid #4e9a06; "
  "  border-bottom: 1px solid #4e9a06; "
  "}"
  ".row.drag-hover image, "
  ".row.drag-hover label { "
  "  color: #4e9a06; "
  "}"
  ".row.drag-hover-top {"
  "  border-top: 1px solid #4e9a06; "
  "}"
  ".row.drag-hover-bottom {"
  "  border-bottom: 1px solid #4e9a06; "
  "}"
;

int
main (int argc, char *argv[])
{
  GtkWidget *window, *list, *sw, *row;
  GtkWidget *hbox, *vbox, *combo, *button;
  gint i;
  gchar *text;
  GtkCssProvider *provider;

  gtk_init (NULL, NULL);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1, NULL);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (), GTK_STYLE_PROVIDER (provider), 800);
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), -1, 300);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_add (GTK_CONTAINER (window), hbox);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  g_object_set (vbox, "margin", 12, NULL);
  gtk_container_add (GTK_CONTAINER (hbox), vbox);

  list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);

  gtk_drag_dest_set (list, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP, entries, 1, GDK_ACTION_MOVE);
  g_signal_connect (list, "drag-data-received", G_CALLBACK (drag_data_received), NULL);
  g_signal_connect (list, "drag-motion", G_CALLBACK (drag_motion), NULL);
  g_signal_connect (list, "drag-leave", G_CALLBACK (drag_leave), NULL);

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

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
