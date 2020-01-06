#include <unistd.h>
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
got_texture (GObject *source,
             GAsyncResult *result,
             gpointer data)
{
  GdkDrop *drop = GDK_DROP (source);
  GtkWidget *image = data;
  const GValue *value;
  GError *error = NULL;

  value = gdk_drop_read_value_finish (drop, result, &error);
  if (value)
    {
      GdkTexture *texture = g_value_get_object (value);
      gtk_image_set_from_paintable (GTK_IMAGE (image), GDK_PAINTABLE (texture));
      gdk_drop_finish (drop, GDK_ACTION_COPY);
    }
  else
    {
      g_error_free (error);
      gdk_drop_finish (drop, 0);
    }

  g_object_set_data (G_OBJECT (image), "drop", NULL);
}

static void
perform_drop (GdkDrop   *drop,
              GtkWidget *image)
{
  if (gdk_drop_has_value (drop, GDK_TYPE_TEXTURE))
    gdk_drop_read_value_async (drop, GDK_TYPE_TEXTURE, G_PRIORITY_DEFAULT, NULL, got_texture, image);
  else
    {
      gdk_drop_finish (drop, 0);
      g_object_set_data (G_OBJECT (image), "drop", NULL);
    }
}

static void
do_copy (GtkWidget *button)
{
  GtkWidget *popover = gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER);
  GtkWidget *image = gtk_popover_get_relative_to (GTK_POPOVER (popover));
  GdkDrop *drop = GDK_DROP (g_object_get_data (G_OBJECT (image), "drop"));

  gtk_popover_popdown (GTK_POPOVER (popover));
  perform_drop (drop, image);
}

static void
do_cancel (GtkWidget *button)
{
  GtkWidget *popover = gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER);
  GtkWidget *image = gtk_popover_get_relative_to (GTK_POPOVER (popover));
  GdkDrop *drop = GDK_DROP (g_object_get_data (G_OBJECT (image), "drop"));

  gtk_popover_popdown (GTK_POPOVER (popover));
  gdk_drop_finish (drop, 0);

  g_object_set_data (G_OBJECT (image), "drop", NULL);
}

static void
ask_actions (GdkDrop *drop,
             GtkWidget *image)
{
  GtkWidget *popover, *box, *button;

  popover = g_object_get_data (G_OBJECT (image), "popover");
  if (!popover)
    {
      popover = gtk_popover_new (image);
      g_object_set_data (G_OBJECT (image), "popover", popover);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (popover), box);
      button = gtk_button_new_with_label ("Copy");
      g_signal_connect (button, "clicked", G_CALLBACK (do_copy), NULL);
      gtk_container_add (GTK_CONTAINER (box), button);
      button = gtk_button_new_with_label ("Move");
      g_signal_connect (button, "clicked", G_CALLBACK (do_copy), NULL);
      gtk_container_add (GTK_CONTAINER (box), button);
      button = gtk_button_new_with_label ("Cancel");
      g_signal_connect (button, "clicked", G_CALLBACK (do_cancel), NULL);
      gtk_container_add (GTK_CONTAINER (box), button);
    }
  gtk_popover_popup (GTK_POPOVER (popover));
}

static gboolean
image_drag_drop (GtkDropTarget    *dest,
                 int               x,
                 int               y,
                 gpointer          data)
{
  GtkWidget *image = data;
  GdkDrop *drop = gtk_drop_target_get_drop (dest);
  GdkDragAction action = gdk_drop_get_actions (drop);

  g_object_set_data_full (G_OBJECT (image), "drop", g_object_ref (drop), g_object_unref);

  g_print ("drop, actions %d\n", action);
  if (!gdk_drag_action_is_unique (action))
    ask_actions (drop, image);
  else
    perform_drop (drop, image);

  return TRUE;
}

static void
update_source_icon (GtkDragSource *source,
                    const char *icon_name,
                    int hotspot)
{
  GdkPaintable *paintable;
  int hot_x, hot_y;
  int size = 48;

  paintable = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                        icon_name, size, 0, NULL);
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
  gtk_drag_source_set_icon (source, paintable, hot_x, hot_y);
  g_object_unref (paintable);
}

static GBytes *
get_data (const char *mimetype,
          gpointer    data)
{
  GtkWidget *image = data;
  GdkContentFormats *formats;
  gboolean want_text;

  formats = gdk_content_formats_new (NULL, 0);
  formats = gtk_content_formats_add_text_targets (formats);
  want_text = gdk_content_formats_contain_mime_type (formats, mimetype);
  gdk_content_formats_unref (formats);

  if (want_text)
    {
      const char *text = gtk_image_get_icon_name (GTK_IMAGE (image));

      return g_bytes_new (text, strlen (text) + 1);
    }
  else if (strcmp (mimetype, "image/png") == 0)
    {
      int size;
      GdkPaintable *paintable = get_image_paintable (GTK_IMAGE (image), &size);
      if (GDK_IS_TEXTURE (paintable))
        {
          char *name = g_strdup ("drag-data-XXXXXX");
          int fd;
          char *data;
          gsize size;

          // FIXME: this is horrible

          fd = g_mkstemp (name);
          close (fd);

          gdk_texture_save_to_png (GDK_TEXTURE (paintable), name);

          g_file_get_contents (name, &data, &size, NULL);
          g_free (name);

          return g_bytes_new_take (data, size);
        }
      
      g_clear_object (&paintable);
    }
  return NULL;
}

static void
drag_begin (GtkDragSource *source)
{
  g_print ("drag begin\n");
}

static void
drag_end (GtkDragSource *source)
{
  g_print ("drag end\n");
}

static gboolean
drag_failed (GtkDragSource       *source,
             GdkDrag             *drag,
             GdkDragCancelReason  reason)
{
  g_print ("drag failed: %d\n", reason);
  return FALSE;
}

GtkWidget *
make_image (const gchar *icon_name, int hotspot)
{
  GtkWidget *image;
  GtkDragSource *source;
  GtkDropTarget *dest;
  GdkContentFormats *formats;
  GdkContentProvider *content;

  image = gtk_image_new_from_icon_name (icon_name);
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);

  formats = gdk_content_formats_new (NULL, 0);
  formats = gtk_content_formats_add_image_targets (formats, FALSE);
  formats = gtk_content_formats_add_text_targets (formats);

  content = gdk_content_provider_new_with_formats (formats, get_data, image);
  source = gtk_drag_source_new (content, GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_ASK);
  g_object_unref (content);
  update_source_icon (source, icon_name, hotspot);

  g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), NULL);
  g_signal_connect (source, "drag-end", G_CALLBACK (drag_end), NULL);
  g_signal_connect (source, "drag-failed", G_CALLBACK (drag_failed), NULL);
  gtk_drag_source_attach (source, image, GDK_BUTTON1_MASK|GDK_BUTTON2_MASK|GDK_BUTTON3_MASK);

  dest = gtk_drop_target_new (formats, GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_ASK);
  g_signal_connect (dest, "drag-drop", G_CALLBACK (image_drag_drop), image);
  gtk_widget_add_controller (image, GTK_EVENT_CONTROLLER (dest));

  gdk_content_formats_unref (formats);

  return image;
}

static void
spinner_drag_begin (GtkDragSource *source,
                    GdkDrag       *drag,
                    GtkWidget     *widget)
{
  GdkPaintable *paintable;

  paintable = gtk_widget_paintable_new (widget);
  gtk_drag_source_set_icon (source, paintable, 0, 0);
  g_object_unref (paintable);
}

static GtkWidget *
make_spinner (void)
{
  GtkWidget *spinner;
  GtkDragSource *source;
  GdkContentProvider *content;
  GValue value = G_VALUE_INIT;

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, "ACTIVE");
  content = gdk_content_provider_new_for_value (&value);
  source = gtk_drag_source_new (content, GDK_ACTION_COPY);
  g_signal_connect (source, "drag-begin", G_CALLBACK (spinner_drag_begin), spinner);
  gtk_drag_source_attach (source, spinner, GDK_BUTTON1_MASK);

  g_object_unref (content);
  g_value_unset (&value);

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

  gtk_grid_attach (GTK_GRID (grid), make_image ("dialog-question", TOP_LEFT), 0, 3, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), make_image ("dialog-information", CENTER), 1, 3, 1, 1);

  gtk_widget_show (window);
  gtk_main ();

  return 0;
}
