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
  cairo_surface_set_device_offset (surface, -x * sx, -y * sy);
  gtk_drag_set_icon_surface (context, surface);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
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
  GtkWidget *row, *handle, *box, *label, *image;

  row = gtk_list_box_row_new ();

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  g_object_set (box, "margin-start", 10, "margin-end", 10, NULL);
  gtk_container_add (GTK_CONTAINER (row), box);

  handle = gtk_event_box_new ();
  image = gtk_image_new_from_icon_name ("open-menu-symbolic", 1);
  gtk_container_add (GTK_CONTAINER (handle), image);
  gtk_container_add (GTK_CONTAINER (box), handle);

  label = gtk_label_new (text);
  gtk_container_add_with_properties (GTK_CONTAINER (box), label, "expand", TRUE, NULL);

  gtk_drag_source_set (handle, GDK_BUTTON1_MASK, entries, 1, GDK_ACTION_MOVE);
  g_signal_connect (handle, "drag-begin", G_CALLBACK (drag_begin), NULL);
  g_signal_connect (handle, "drag-data-get", G_CALLBACK (drag_data_get), NULL);

  gtk_drag_dest_set (row, GTK_DEST_DEFAULT_ALL, entries, 1, GDK_ACTION_MOVE);
  g_signal_connect (row, "drag-data-received", G_CALLBACK (drag_data_received), NULL);

  return row;
}

static const char *css =
  ".drag-icon { "
  "  background: white; "
  "  border: 1px solid black; "
  "}";

int
main (int argc, char *argv[])
{
  GtkWidget *window, *list, *sw, *row;
  gint i;
  gchar *text;
  GtkCssProvider *provider;

  gtk_init (NULL, NULL);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1, NULL);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), -1, 300);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_container_add (GTK_CONTAINER (window), sw);

  list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
  gtk_container_add (GTK_CONTAINER (sw), list);

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
