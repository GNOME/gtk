#include <string.h>
#include "iconbrowserapp.h"
#include "iconbrowserwin.h"
#include "iconstore.h"
#include <gtk/gtk.h>

/* Drag 'n Drop */

typedef struct
{
  gchar *id;
  gchar *name;
  gchar *description;
} Context;

static void
context_free (gpointer data)
{
  Context *context = data;

  g_free (context->id);
  g_free (context->name);
  g_free (context->description);
  g_free (context);
}

struct _IconBrowserWindow
{
  GtkApplicationWindow parent;
  GHashTable *contexts;

  GtkWidget *context_list;
  Context *current_context;
  gboolean symbolic;
  GtkWidget *symbolic_radio;
  GtkTreeModelFilter *filter_model;
  GtkWidget *details;

  GtkListStore *store;
  GtkCellRenderer *cell;
  GtkCellRenderer *text_cell;
  GtkWidget *search;
  GtkWidget *searchbar;
  GtkWidget *searchentry;
  GtkWidget *list;
  GtkWidget *image1;
  GtkWidget *image2;
  GtkWidget *image3;
  GtkWidget *image4;
  GtkWidget *image5;
  GtkWidget *image6;
  GtkWidget *image7;
  GtkWidget *image8;
  GtkWidget *label8;
  GtkWidget *description;
};

struct _IconBrowserWindowClass
{
  GtkApplicationWindowClass parent_class;
};

G_DEFINE_TYPE(IconBrowserWindow, icon_browser_window, GTK_TYPE_APPLICATION_WINDOW);

static GtkIconTheme *
icon_browser_window_get_icon_theme (IconBrowserWindow *win)
{
  return gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (win)));
}

static void
search_text_changed (GtkEntry *entry, IconBrowserWindow *win)
{
  const gchar *text;

  text = gtk_editable_get_text (GTK_EDITABLE (entry));

  if (text[0] == '\0')
    return;

  gtk_tree_model_filter_refilter (win->filter_model);
}

static void
set_image (GtkWidget *image, const gchar *name, gint size)
{
  gtk_image_set_from_icon_name (GTK_IMAGE (image), name);
  gtk_image_set_pixel_size (GTK_IMAGE (image), size);
}

static void
item_activated (GtkIconView *icon_view, GtkTreePath *path, IconBrowserWindow *win)
{
  GtkIconTheme *icon_theme = icon_browser_window_get_icon_theme (win);
  GtkTreeIter iter;
  gchar *name;
  gchar *description;
  gint column;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (win->filter_model), &iter, path);

  if (win->symbolic)
    column = ICON_STORE_SYMBOLIC_NAME_COLUMN;
  else
    column = ICON_STORE_NAME_COLUMN;
  gtk_tree_model_get (GTK_TREE_MODEL (win->filter_model), &iter,
                      column, &name,
                      ICON_STORE_DESCRIPTION_COLUMN, &description,
                      -1);

  if (name == NULL || !gtk_icon_theme_has_icon (icon_theme, name))
    {
      g_free (description);
      return;
    }

  gtk_window_set_title (GTK_WINDOW (win->details), name);
  set_image (win->image1, name, 8);
  set_image (win->image2, name, 16);
  set_image (win->image3, name, 18);
  set_image (win->image4, name, 24);
  set_image (win->image5, name, 32);
  set_image (win->image6, name, 48);
  set_image (win->image7, name, 64);
  if (win->symbolic)
    {
      gtk_widget_show (win->image8);
      gtk_widget_show (win->label8);
      set_image (win->image8, name, 64);
    }
  else
    {
      gtk_widget_hide (win->image8);
      gtk_widget_hide (win->label8);
    }
  if (description && description[0])
    {
      gtk_label_set_text (GTK_LABEL (win->description), description);
      gtk_widget_show (win->description);
    }
  else
    {
      gtk_widget_hide (win->description);
    }

  gtk_window_present (GTK_WINDOW (win->details));

  g_free (name);
  g_free (description);
}

static void
add_icon (IconBrowserWindow *win,
          const gchar       *name,
          const gchar       *description,
          const gchar       *context)
{
  GtkIconTheme *icon_theme = icon_browser_window_get_icon_theme (win);
  gchar *regular_name;
  gchar *symbolic_name;

  regular_name = g_strdup (name);
  if (!gtk_icon_theme_has_icon (icon_theme, regular_name))
    {
      g_free (regular_name);
      regular_name = NULL;
    }

  symbolic_name = g_strconcat (name, "-symbolic", NULL);
  if (!gtk_icon_theme_has_icon (icon_theme, symbolic_name))
    {
      g_free (symbolic_name);
      symbolic_name = NULL;
    }

  gtk_list_store_insert_with_values (win->store, NULL, -1,
                                     ICON_STORE_NAME_COLUMN, regular_name,
                                     ICON_STORE_SYMBOLIC_NAME_COLUMN, symbolic_name,
                                     ICON_STORE_DESCRIPTION_COLUMN, description,
                                     ICON_STORE_CONTEXT_COLUMN, context,
                                     -1);
}

static void
add_context (IconBrowserWindow *win,
             const gchar       *id,
             const gchar       *name,
             const gchar       *description)
{
  Context *c;
  GtkWidget *row;

  c = g_new (Context, 1);
  c->id = g_strdup (id);
  c->name = g_strdup (name);
  c->description = g_strdup (description);

  g_hash_table_insert (win->contexts, c->id, c);

  row = gtk_label_new (name);
  gtk_label_set_xalign (GTK_LABEL (row), 0);
  g_object_set_data (G_OBJECT (row), "context", c);
  gtk_widget_show (row);
  g_object_set (row, "margin", 10, NULL);

  gtk_list_box_insert (GTK_LIST_BOX (win->context_list), row, -1);

  /* set the tooltip on the list box row */
  row = gtk_widget_get_parent (row);
  gtk_widget_set_tooltip_text (row, description);

  if (win->current_context == NULL)
    win->current_context = c;
}

static void
selected_context_changed (GtkListBox *list, IconBrowserWindow *win)
{
  GtkWidget *row;
  GtkWidget *label;

  row = GTK_WIDGET (gtk_list_box_get_selected_row (list));
  if (row == NULL)
    return;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (win->search), FALSE);

  label = gtk_bin_get_child (GTK_BIN (row));
  win->current_context = g_object_get_data (G_OBJECT (label), "context");
  gtk_tree_model_filter_refilter (win->filter_model);
}

static void
populate (IconBrowserWindow *win)
{
  GFile *file;
  GKeyFile *kf;
  char *data;
  gsize length;
  char **groups;
  int i;

  file = g_file_new_for_uri ("resource:/org/gtk/iconbrowser/gtk/icon.list");
  g_file_load_contents (file, NULL, &data, &length, NULL, NULL);

  kf = g_key_file_new ();
  g_key_file_load_from_data (kf, data, length, G_KEY_FILE_NONE, NULL);

  groups = g_key_file_get_groups (kf, &length);
  for (i = 0; i < length; i++)
    {
      const char *context;
      const char *name;
      const char *description;
      char **keys;
      gsize len;
      int j;

      context = groups[i];
      name = g_key_file_get_string (kf, context, "Name", NULL);
      description = g_key_file_get_string (kf, context, "Description", NULL);
      add_context (win, context, name, description);

      keys = g_key_file_get_keys (kf, context, &len, NULL);
      for (j = 0; j < len; j++)
        {
          const char *key = keys[j];
          const char *value;

          if (strcmp (key, "Name") == 0 || strcmp (key, "Description") == 0)
            continue;

          value = g_key_file_get_string (kf, context, key, NULL);

          add_icon (win, key, value, context);
        }
      g_strfreev (keys);
    }
  g_strfreev (groups);
}

static void
copy_to_clipboard (GtkButton         *button,
                   IconBrowserWindow *win)
{
  GdkClipboard *clipboard;

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (win));
  gdk_clipboard_set_text (clipboard, gtk_window_get_title (GTK_WINDOW (win->details)));
}

static gboolean
icon_visible_func (GtkTreeModel *model,
                   GtkTreeIter  *iter,
                   gpointer      data)
{
  IconBrowserWindow *win = data;
  gchar *context;
  gchar *name;
  gint column;
  gboolean search;
  const gchar *search_text;
  gboolean visible;

  search = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->search));
  search_text = gtk_editable_get_text (GTK_EDITABLE (win->searchentry));

  if (win->symbolic)
    column = ICON_STORE_SYMBOLIC_NAME_COLUMN;
  else
    column = ICON_STORE_NAME_COLUMN;

  gtk_tree_model_get (model, iter,
                      column, &name,
                      ICON_STORE_CONTEXT_COLUMN, &context,
                      -1);
  if (!name)
    visible = FALSE;
  else if (search)
    visible = strstr (name, search_text) != NULL;
  else
    visible = win->current_context != NULL && g_strcmp0 (context, win->current_context->id) == 0;

  g_free (name);
  g_free (context);

  return visible;
}

static void
symbolic_toggled (GtkToggleButton *toggle, IconBrowserWindow *win)
{
  gint column;

  win->symbolic = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));

  if (win->symbolic)
    column = ICON_STORE_SYMBOLIC_NAME_COLUMN;
  else
    column = ICON_STORE_NAME_COLUMN;

  icon_store_set_text_column (ICON_STORE (win->store), column);

  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (win->list), win->cell, "icon-name", column, NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (win->list), win->text_cell, "text", column, NULL);

  gtk_tree_model_filter_refilter (win->filter_model);
  gtk_widget_queue_draw (win->list);
}

static void
search_mode_toggled (GObject *searchbar, GParamSpec *pspec, IconBrowserWindow *win)
{
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (searchbar)))
    gtk_list_box_unselect_all (GTK_LIST_BOX (win->context_list));
}

static GdkPaintable *
get_image_paintable (GtkImage *image)
{
  const gchar *icon_name;
  GtkIconTheme *icon_theme;
  GtkIconPaintable *icon;
  int size;

  switch (gtk_image_get_storage_type (image))
    {
    case GTK_IMAGE_PAINTABLE:
      return g_object_ref (gtk_image_get_paintable (image));
    case GTK_IMAGE_ICON_NAME:
      icon_name = gtk_image_get_icon_name (image);
      size = gtk_image_get_pixel_size (image);
      icon_theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (image)));
      icon = gtk_icon_theme_lookup_icon (icon_theme,
                                         icon_name,
                                         NULL,
                                         size, 1,
                                         gtk_widget_get_direction (GTK_WIDGET (image)),
                                         0);
      if (icon == NULL)
        return NULL;
      return GDK_PAINTABLE (icon);
    default:
      g_warning ("Image storage type %d not handled",
                 gtk_image_get_storage_type (image));
      return NULL;
    }
}

static void
drag_begin (GtkDragSource *source,
            GdkDrag       *drag,
            GtkWidget     *widget)
{
  GdkPaintable *paintable;

  paintable = get_image_paintable (GTK_IMAGE (widget));
  if (paintable)
    {
      int w, h;

      w = gdk_paintable_get_intrinsic_width (paintable);
      h = gdk_paintable_get_intrinsic_height (paintable);
      gtk_drag_source_set_icon (source, paintable, w, h);
      g_object_unref (paintable);
    }
}

static GdkContentProvider *
drag_prepare_texture (GtkDragSource *source,
                      double         x,
                      double         y,
                      GtkWidget     *widget)
{
  GdkPaintable *paintable = get_image_paintable (GTK_IMAGE (widget));

  if (!GDK_IS_TEXTURE (paintable))
    return NULL;

  return gdk_content_provider_new_typed (GDK_TYPE_TEXTURE, paintable);
}

static GdkContentProvider *
drag_prepare_file (GtkDragSource *source,
                   double         x,
                   double         y,
                   GtkWidget     *widget)
{
  GdkContentProvider *content;
  GtkIconTheme *icon_theme;
  const char *name;
  GtkIconPaintable *info;

  name = gtk_image_get_icon_name (GTK_IMAGE (widget));
  icon_theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (widget));

  info = gtk_icon_theme_lookup_icon (icon_theme,
                                     name,
                                     NULL,
                                     32, 1,
                                     gtk_widget_get_direction (widget),
                                     0);
  content = gdk_content_provider_new_typed (G_TYPE_FILE,  gtk_icon_paintable_get_file (info));
  g_object_unref (info);

  return content;
}

static void
setup_image_dnd (GtkWidget *image)
{
  GtkDragSource *source;

  source = gtk_drag_source_new ();
  g_signal_connect (source, "prepare", G_CALLBACK (drag_prepare_texture), image);
  g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), image);
  gtk_widget_add_controller (image, GTK_EVENT_CONTROLLER (source));
}

static void
setup_scalable_image_dnd (GtkWidget *image)
{
  GtkDragSource *source;

  source = gtk_drag_source_new ();
  g_signal_connect (source, "prepare", G_CALLBACK (drag_prepare_file), image);
  g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), image);
  gtk_widget_add_controller (image, GTK_EVENT_CONTROLLER (source));
}

static void
icon_browser_window_init (IconBrowserWindow *win)
{
  GdkContentFormats *list;

  gtk_widget_init_template (GTK_WIDGET (win));

  list = gdk_content_formats_new_for_gtype (G_TYPE_STRING);
  gtk_icon_view_enable_model_drag_source (GTK_ICON_VIEW (win->list),
                                          GDK_BUTTON1_MASK,
                                          list,
                                          GDK_ACTION_COPY);
  gdk_content_formats_unref (list);

  setup_image_dnd (win->image1);
  setup_image_dnd (win->image2);
  setup_image_dnd (win->image3);
  setup_image_dnd (win->image4);
  setup_image_dnd (win->image5);
  setup_image_dnd (win->image6);
  setup_image_dnd (win->image7);
  setup_scalable_image_dnd (win->image8);

  win->contexts = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, context_free);

  gtk_tree_model_filter_set_visible_func (win->filter_model, icon_visible_func, win, NULL);
  gtk_window_set_transient_for (GTK_WINDOW (win->details), GTK_WINDOW (win));

  g_signal_connect (win->searchbar, "notify::search-mode-enabled",
                    G_CALLBACK (search_mode_toggled), win);
  gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (win->searchbar),
                                         GTK_WIDGET (win));

  symbolic_toggled (GTK_TOGGLE_BUTTON (win->symbolic_radio), win);

  populate (win);
}

static void
icon_browser_window_finalize (GObject *object)
{
  IconBrowserWindow *win = ICON_BROWSER_WINDOW (object);

  g_hash_table_unref (win->contexts);

  G_OBJECT_CLASS (icon_browser_window_parent_class)->finalize (object);
}

static void
icon_browser_window_class_init (IconBrowserWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = icon_browser_window_finalize;

  g_type_ensure (ICON_STORE_TYPE);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/iconbrowser/gtk/window.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, context_list);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, filter_model);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, symbolic_radio);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, details);

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, store);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, cell);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, text_cell);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, search);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, searchbar);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, searchentry);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, list);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image1);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image2);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image3);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image4);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image5);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image6);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image7);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, image8);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, label8);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, description);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), search_text_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), item_activated);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), selected_context_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), symbolic_toggled);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), copy_to_clipboard);
}

IconBrowserWindow *
icon_browser_window_new (IconBrowserApp *app)
{
  return g_object_new (ICON_BROWSER_WINDOW_TYPE, "application", app, NULL);
}
