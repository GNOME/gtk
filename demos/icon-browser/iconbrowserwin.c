#include <string.h>
#include "iconbrowserapp.h"
#include "iconbrowserwin.h"
#include "iconstore.h"
#include <gtk/gtk.h>

/* Drag 'n Drop */
static GtkTargetEntry target_table[] = {
  { "text/uri-list", 0, 0 },
};

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
  GtkWidget *label6;
  GtkWidget *description;
};

struct _IconBrowserWindowClass
{
  GtkApplicationWindowClass parent_class;
};

G_DEFINE_TYPE(IconBrowserWindow, icon_browser_window, GTK_TYPE_APPLICATION_WINDOW);

static void
search_text_changed (GtkEntry *entry, IconBrowserWindow *win)
{
  const gchar *text;

  text = gtk_entry_get_text (entry);

  if (text[0] == '\0')
    return;

  gtk_tree_model_filter_refilter (win->filter_model);
}

static GdkPixbuf *
get_icon (GtkWidget *image, const gchar *name, gint size)
{
  GtkIconInfo *info;
  GtkStyleContext *context;
  GdkPixbuf *pixbuf;

  context = gtk_widget_get_style_context (image);
  info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (), name, size, 0);
  pixbuf = gtk_icon_info_load_symbolic_for_context (info, context, NULL, NULL);
  g_object_unref (info);

  return pixbuf;
}

static void
set_image (GtkWidget *image, const gchar *name, gint size)
{
  GdkPixbuf *pixbuf;

  gtk_image_set_from_icon_name (GTK_IMAGE (image), name, 1);
  gtk_image_set_pixel_size (GTK_IMAGE (image), size);
  pixbuf = get_icon (image, name, size);
  gtk_drag_source_set_icon_pixbuf (gtk_widget_get_parent (image), pixbuf);
  g_object_unref (pixbuf);
}

static void
item_activated (GtkIconView *icon_view, GtkTreePath *path, IconBrowserWindow *win)
{
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

  if (name == NULL || !gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), name))
    {
      g_free (description);
      return;
    }

  gtk_window_set_title (GTK_WINDOW (win->details), name);
  set_image (win->image1, name, 16);
  set_image (win->image2, name, 24);
  set_image (win->image3, name, 32);
  set_image (win->image4, name, 48);
  set_image (win->image5, name, 64);
  if (win->symbolic)
    {
      gtk_widget_show (win->image6);
      gtk_widget_show (win->label6);
      gtk_widget_show (gtk_widget_get_parent (win->image6));
      set_image (win->image6, name, 64);
    }
  else
    {
      gtk_widget_hide (win->image6);
      gtk_widget_hide (win->label6);
      gtk_widget_hide (gtk_widget_get_parent (win->image6));
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
  gchar *regular_name;
  gchar *symbolic_name;

  regular_name = g_strdup (name);
  if (!gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), regular_name))
    {
      g_free (regular_name);
      regular_name = NULL;
    }

  symbolic_name = g_strconcat (name, "-symbolic", NULL);
  if (!gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), symbolic_name))
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

static gboolean
key_press_event_cb (GtkWidget *widget,
                    GdkEvent  *event,
                    gpointer   data)
{
  IconBrowserWindow *win = data;

  return gtk_search_bar_handle_event (GTK_SEARCH_BAR (win->searchbar), event);
}

static void
copy_to_clipboard (GtkButton         *button,
                   IconBrowserWindow *win)
{
  GtkClipboard *clipboard;

  clipboard = gtk_clipboard_get_default (gdk_display_get_default ());
  gtk_clipboard_set_text (clipboard, gtk_window_get_title (GTK_WINDOW (win->details)), -1);
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
  search_text = gtk_entry_get_text (GTK_ENTRY (win->searchentry));

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

static void
get_image_data (GtkWidget        *widget,
                GdkDragContext   *context,
                GtkSelectionData *selection,
                guint             target_info,
                guint             time,
                gpointer          data)
{
  GtkWidget *image;
  const gchar *name;
  gint size;
  GdkPixbuf *pixbuf;

  image = gtk_bin_get_child (GTK_BIN (widget));

  gtk_image_get_icon_name (GTK_IMAGE (image), &name, NULL);
  size = gtk_image_get_pixel_size (GTK_IMAGE (image));

  pixbuf = get_icon (image, name, size);
  gtk_selection_data_set_pixbuf (selection, pixbuf);
  g_object_unref (pixbuf);
}

static void
get_scalable_image_data (GtkWidget        *widget,
                         GdkDragContext   *context,
                         GtkSelectionData *selection,
                         guint             target_info,
                         guint             time,
                         gpointer          data)
{
  gchar *uris[2];
  GtkIconInfo *info;
  GtkWidget *image;
  GFile *file;
  const gchar *name;

  image = gtk_bin_get_child (GTK_BIN (widget));
  gtk_image_get_icon_name (GTK_IMAGE (image), &name, NULL);

  info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (), name, -1, 0);
  file = g_file_new_for_path (gtk_icon_info_get_filename (info));
  uris[0] = g_file_get_uri (file);
  uris[1] = NULL;

  gtk_selection_data_set_uris (selection, uris);

  g_free (uris[0]);
  g_object_unref (info);
  g_object_unref (file);
}

static void
setup_image_dnd (GtkWidget *image)
{
  GtkWidget *parent;

  parent = gtk_widget_get_parent (image);
  gtk_drag_source_set (parent, GDK_BUTTON1_MASK, NULL, 0, GDK_ACTION_COPY);
  gtk_drag_source_add_image_targets (parent);
  g_signal_connect (parent, "drag-data-get", G_CALLBACK (get_image_data), NULL);
}

static void
setup_scalable_image_dnd (GtkWidget *image)
{
  GtkWidget *parent;

  parent = gtk_widget_get_parent (image);
  gtk_drag_source_set (parent, GDK_BUTTON1_MASK,
                       target_table, G_N_ELEMENTS (target_table),
                       GDK_ACTION_COPY);

  g_signal_connect (parent, "drag-data-get", G_CALLBACK (get_scalable_image_data), NULL);
}

static void
icon_browser_window_init (IconBrowserWindow *win)
{
  GtkTargetList *list;
  GtkTargetEntry *targets;
  gint n_targets;

  gtk_widget_init_template (GTK_WIDGET (win));

  list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_text_targets (list, 0);
  targets = gtk_target_table_new_from_list (list, &n_targets);
  gtk_target_list_unref (list);

  gtk_icon_view_enable_model_drag_source (GTK_ICON_VIEW (win->list),
                                          GDK_BUTTON1_MASK,
                                          targets, n_targets,
                                          GDK_ACTION_COPY);

  gtk_target_table_free (targets, n_targets);

  setup_image_dnd (win->image1);
  setup_image_dnd (win->image2);
  setup_image_dnd (win->image3);
  setup_image_dnd (win->image4);
  setup_image_dnd (win->image5);
  setup_scalable_image_dnd (win->image6);

  win->contexts = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, context_free);

  gtk_tree_model_filter_set_visible_func (win->filter_model, icon_visible_func, win, NULL);
  gtk_window_set_transient_for (GTK_WINDOW (win->details), GTK_WINDOW (win));

  g_signal_connect (win->searchbar, "notify::search-mode-enabled",
                    G_CALLBACK (search_mode_toggled), win);

  symbolic_toggled (GTK_TOGGLE_BUTTON (win->symbolic_radio), win);

  populate (win);
}

static void
icon_browser_window_class_init (IconBrowserWindowClass *class)
{
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
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, label6);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), IconBrowserWindow, description);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), search_text_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), item_activated);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), selected_context_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), symbolic_toggled);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), key_press_event_cb);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), copy_to_clipboard);
}

IconBrowserWindow *
icon_browser_window_new (IconBrowserApp *app)
{
  return g_object_new (ICON_BROWSER_WINDOW_TYPE, "application", app, NULL);
}
