#include <gtk/gtk.h>

static GdkPaintable *
get_image_paintable (GtkImage *image,
                    int      *out_size)
{
  GtkIconTheme *icon_theme;
  const char *icon_name;
  int width = 48;
  GdkPaintable *paintable;
  GtkIconInfo *icon_info;

  switch (gtk_image_get_storage_type (image))
    {
    case GTK_IMAGE_PAINTABLE:
      paintable = gtk_image_get_paintable (image);
      *out_size = gdk_paintable_get_intrinsic_width (paintable);
      return g_object_ref (paintable);
    case GTK_IMAGE_ICON_NAME:
      icon_name = gtk_image_get_icon_name (image);
      icon_theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (image)));
      *out_size = width;
      icon_info = gtk_icon_theme_lookup_icon (icon_theme, icon_name, width, GTK_ICON_LOOKUP_GENERIC_FALLBACK);
      paintable = gtk_icon_info_load_icon (icon_info, NULL);
      g_object_unref (icon_info);
      return paintable;
    default:
      g_warning ("Image storage type %d not handled",
                 gtk_image_get_storage_type (image));
      return NULL;
    }
}

enum {
  TOP_LEFT,
  CENTER,
  BOTTOM_RIGHT
};

static void
image_drag_begin (GtkWidget      *widget,
                  GdkDrag        *drag,
                  gpointer        data)
{
  GdkPaintable *paintable;
  gint hotspot;
  gint hot_x, hot_y;
  gint size;

  paintable = get_image_paintable (GTK_IMAGE (data), &size);
  hotspot = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (data), "hotspot"));
  switch (hotspot)
    {
    default:
    case TOP_LEFT:
      hot_x = 0;
      hot_y = 0;
      break;
    case CENTER:
      hot_x = size / 2;
      hot_y = size / 2;
      break;
    case BOTTOM_RIGHT:
      hot_x = size;
      hot_y = size;
      break;
    }
  gtk_drag_set_icon_paintable (drag, paintable, hot_x, hot_y);
  g_object_unref (paintable);
}

static void
drag_widget_destroyed (GtkWidget *image, gpointer data)
{
  GtkWidget *widget = data;

  g_print ("drag widget destroyed\n");
  g_object_unref (image);
  g_object_set_data (G_OBJECT (widget), "drag widget", NULL);
}

static void
window_drag_end (GtkWidget *widget,
                 GdkDrag   *drag,
                 gpointer   data)
{
  GtkWidget *window = data;

  gtk_widget_destroy (window);
  g_signal_handlers_disconnect_by_func (widget, window_drag_end, data);
}

static void
window_drag_begin (GtkWidget      *widget,
                   GdkDrag        *drag,
                   gpointer        data)
{
  GdkPaintable *paintable;
  GtkWidget *image;
  int hotspot;
  int size;

  hotspot = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (data), "hotspot"));

  image = g_object_get_data (G_OBJECT (widget), "drag widget");
  if (image == NULL)
    {
      g_print ("creating new drag widget\n");
      paintable = get_image_paintable (GTK_IMAGE (data), &size);
      image = gtk_image_new_from_paintable (paintable);
      g_object_unref (paintable);
      g_object_ref (image);
      g_object_set_data (G_OBJECT (widget), "drag widget", image);
      g_signal_connect (image, "destroy", G_CALLBACK (drag_widget_destroyed), widget);
    }
  else
    g_print ("reusing drag widget\n");

  gtk_drag_set_icon_widget (drag, image, 0, 0);

  if (hotspot == CENTER)
    g_signal_connect (widget, "drag-end", G_CALLBACK (window_drag_end), image);
}

static void
update_source_target_list (GtkWidget *image)
{
  GdkContentFormats *target_list;

  target_list = gdk_content_formats_new (NULL, 0);

  target_list = gtk_content_formats_add_image_targets (target_list, FALSE);
  if (gtk_image_get_storage_type (GTK_IMAGE (image)) == GTK_IMAGE_ICON_NAME)
    target_list = gtk_content_formats_add_text_targets (target_list);

  gtk_drag_source_set_target_list (image, target_list);

  gdk_content_formats_unref (target_list);
}

static void
update_dest_target_list (GtkWidget *image)
{
  GdkContentFormats *target_list;

  target_list = gdk_content_formats_new (NULL, 0);

  target_list = gtk_content_formats_add_image_targets (target_list, FALSE);
  target_list = gtk_content_formats_add_text_targets (target_list);

  gtk_drag_dest_set_target_list (image, target_list);

  gdk_content_formats_unref (target_list);
}

void
image_drag_data_get (GtkWidget        *widget,
                     GdkDrag          *drag,
                     GtkSelectionData *selection_data,
                     gpointer          data)
{
  GdkPaintable *paintable;
  const gchar *name;
  int size;

  if (gtk_selection_data_targets_include_image (selection_data, TRUE))
    {
      paintable = get_image_paintable (GTK_IMAGE (data), &size);
      if (GDK_IS_TEXTURE (paintable))
        gtk_selection_data_set_texture (selection_data, GDK_TEXTURE (paintable));
      if (paintable)
        g_object_unref (paintable);
    }
  else if (gtk_selection_data_targets_include_text (selection_data))
    {
      if (gtk_image_get_storage_type (GTK_IMAGE (data)) == GTK_IMAGE_ICON_NAME)
        name = gtk_image_get_icon_name (GTK_IMAGE (data));
      else
        name = "Boo!";
      gtk_selection_data_set_text (selection_data, name, -1);
    }
  else
    {
      g_assert_not_reached ();
    }
}

static void
image_drag_data_received (GtkWidget        *widget,
                          GdkDrop          *drop,
                          GtkSelectionData *selection_data,
                          gpointer          data)
{
  gchar *text;

  if (gtk_selection_data_get_length (selection_data) == 0)
    return;

  if (gtk_selection_data_targets_include_image (selection_data, FALSE))
    {
      GdkTexture *texture;

      texture = gtk_selection_data_get_texture (selection_data);
      gtk_image_set_from_paintable (GTK_IMAGE (data), GDK_PAINTABLE (texture));

      g_object_unref (texture);
    }
  else if (gtk_selection_data_targets_include_text (selection_data))
    {
      text = (gchar *)gtk_selection_data_get_text (selection_data);
      gtk_image_set_from_icon_name (GTK_IMAGE (data), text);
      g_free (text);
    }
  else
    {
      g_assert_not_reached ();
    }
}


GtkWidget *
make_image (const gchar *icon_name, int hotspot)
{
  GtkWidget *image;

  image = gtk_image_new_from_icon_name (icon_name);
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);

  gtk_drag_source_set (image, GDK_BUTTON1_MASK, NULL, GDK_ACTION_COPY);
  update_source_target_list (image);

  g_object_set_data  (G_OBJECT (image), "hotspot", GINT_TO_POINTER (hotspot));

  g_signal_connect (image, "drag-begin", G_CALLBACK (image_drag_begin), image);
  g_signal_connect (image, "drag-data-get", G_CALLBACK (image_drag_data_get), image);

  gtk_drag_dest_set (image, GTK_DEST_DEFAULT_ALL, NULL, GDK_ACTION_COPY);
  g_signal_connect (image, "drag-data-received", G_CALLBACK (image_drag_data_received), image);
  update_dest_target_list (image);

  return image;
}

GtkWidget *
make_image2 (const gchar *icon_name, int hotspot)
{
  GtkWidget *image;

  image = gtk_image_new_from_icon_name (icon_name);
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);

  gtk_drag_source_set (image, GDK_BUTTON1_MASK, NULL, GDK_ACTION_COPY);
  update_source_target_list (image);

  g_object_set_data  (G_OBJECT (image), "hotspot", GINT_TO_POINTER (hotspot));

  g_signal_connect (image, "drag-begin", G_CALLBACK (window_drag_begin), image);
  g_signal_connect (image, "drag-data-get", G_CALLBACK (image_drag_data_get), image);

  gtk_drag_dest_set (image, GTK_DEST_DEFAULT_ALL, NULL, GDK_ACTION_COPY);
  g_signal_connect (image, "drag-data-received", G_CALLBACK (image_drag_data_received), image);
  update_dest_target_list (image);

  return image;
}

static void
spinner_drag_begin (GtkWidget      *widget,
                    GdkDrag        *drag,
                    gpointer        data)
{
  GtkWidget *spinner;

  g_print ("GtkWidget::drag-begin\n");
  spinner = g_object_new (GTK_TYPE_SPINNER,
                          "visible", TRUE,
                          "active",  TRUE,
                          NULL);
  gtk_drag_set_icon_widget (drag, spinner, 0, 0);
  g_object_set_data (G_OBJECT (drag), "spinner", spinner);
}

static void
spinner_drag_end (GtkWidget      *widget,
                  GdkDrag        *drag,
                  gpointer        data)
{
  GtkWidget *spinner;

  g_print ("GtkWidget::drag-end\n");
  spinner = g_object_get_data (G_OBJECT (drag), "spinner");
  gtk_widget_destroy (spinner);
}

static gboolean
spinner_drag_failed (GtkWidget      *widget,
                     GdkDrag        *drag,
                     GtkDragResult   result,
                     gpointer        data)
{
  GTypeClass *class;
  GEnumValue *value;

  class = g_type_class_ref (GTK_TYPE_DRAG_RESULT);
  value = g_enum_get_value (G_ENUM_CLASS (class), result);
  g_print ("GtkWidget::drag-failed %s\n", value->value_nick);
  g_type_class_unref (class);

  return FALSE;
}

void
spinner_drag_data_get (GtkWidget        *widget,
                       GdkDrag          *drag,
                       GtkSelectionData *selection_data,
                       gpointer          data)
{
  g_print ("GtkWidget::drag-data-get\n");
  gtk_selection_data_set_text (selection_data, "ACTIVE", -1);
}

static GtkWidget *
make_spinner (void)
{
  GtkWidget *spinner;

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));

  gtk_drag_source_set (spinner, GDK_BUTTON1_MASK, NULL, GDK_ACTION_COPY);
  gtk_drag_source_add_text_targets (spinner);

  g_signal_connect (spinner, "drag-begin", G_CALLBACK (spinner_drag_begin), spinner);
  g_signal_connect (spinner, "drag-end", G_CALLBACK (spinner_drag_end), spinner);
  g_signal_connect (spinner, "drag-failed", G_CALLBACK (spinner_drag_failed), spinner);
  g_signal_connect (spinner, "drag-data-get", G_CALLBACK (spinner_drag_data_get), spinner);

  return spinner;
}

int
main (int argc, char *Argv[])
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *entry;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Drag And Drop");
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  grid = gtk_grid_new ();
  g_object_set (grid,
                "margin", 20,
                "row-spacing", 20,
                "column-spacing", 20,
                NULL);
  gtk_container_add (GTK_CONTAINER (window), grid);
  gtk_grid_attach (GTK_GRID (grid), make_image ("dialog-warning", TOP_LEFT), 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), make_image ("process-stop", BOTTOM_RIGHT), 1, 0, 1, 1);

  entry = gtk_entry_new ();
  gtk_grid_attach (GTK_GRID (grid), entry, 0, 1, 2, 1);

  gtk_grid_attach (GTK_GRID (grid), make_spinner (), 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), make_image ("weather-clear", CENTER), 1, 2, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), make_image2 ("dialog-question", TOP_LEFT), 0, 3, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), make_image2 ("dialog-information", CENTER), 1, 3, 1, 1);

  gtk_widget_show (window);
  gtk_main ();

  return 0;
}
