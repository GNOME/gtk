/* simple.c
 * Copyright (C) 2017  Red Hat, Inc
 * Author: Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#include <gtk/gtk.h>


#define STRING_TYPE_HOLDER (string_holder_get_type ())
G_DECLARE_FINAL_TYPE (StringHolder, string_holder, STRING, HOLDER, GObject)

struct _StringHolder {
  GObject parent_instance;
  char *title;
  char *icon;
  char *description;
};

G_DEFINE_TYPE (StringHolder, string_holder, G_TYPE_OBJECT);

static void
string_holder_init (StringHolder *holder)
{
}

static void
string_holder_finalize (GObject *object)
{
  StringHolder *holder = STRING_HOLDER (object);

  g_free (holder->title);
  g_free (holder->icon);
  g_free (holder->description);

  G_OBJECT_CLASS (string_holder_parent_class)->finalize (object);
}

static void
string_holder_class_init (StringHolderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = string_holder_finalize;
}

static StringHolder *
string_holder_new (const char *title, const char *icon, const char *description)
{
  StringHolder *holder = g_object_new (STRING_TYPE_HOLDER, NULL);
  holder->title = g_strdup (title);
  holder->icon = g_strdup (icon);
  holder->description = g_strdup (description);
  return holder;
}

static void
strings_setup_item_single_line (GtkSignalListItemFactory *factory,
                                GtkListItem              *item)
{
  GtkWidget *box, *image, *title;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

  image = gtk_image_new ();
  title = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (title), 0.0);

  gtk_box_append (GTK_BOX (box), image);
  gtk_box_append (GTK_BOX (box), title);

  g_object_set_data (G_OBJECT (item), "title", title);
  g_object_set_data (G_OBJECT (item), "image", image);

  gtk_list_item_set_child (item, box);
}

static void
strings_setup_item_full (GtkSignalListItemFactory *factory,
                         GtkListItem              *item)
{
  GtkWidget *box, *box2, *image, *title, *description;

  image = gtk_image_new ();
  title = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (title), 0.0);
  description = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (description), 0.0);
  gtk_style_context_add_class (gtk_widget_get_style_context (description), "dim-label");

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  gtk_box_append (GTK_BOX (box), image);
  gtk_box_append (GTK_BOX (box), box2);
  gtk_box_append (GTK_BOX (box2), title);
  gtk_box_append (GTK_BOX (box2), description);

  g_object_set_data (G_OBJECT (item), "title", title);
  g_object_set_data (G_OBJECT (item), "image", image);
  g_object_set_data (G_OBJECT (item), "description", description);

  gtk_list_item_set_child (item, box);
}

static void
strings_bind_item (GtkSignalListItemFactory *factory,
                    GtkListItem              *item)
{
  GtkWidget *image, *title, *description;
  StringHolder *holder;

  holder = gtk_list_item_get_item (item);

  title = g_object_get_data (G_OBJECT (item), "title");
  image = g_object_get_data (G_OBJECT (item), "image");
  description = g_object_get_data (G_OBJECT (item), "description");

  gtk_label_set_label (GTK_LABEL (title), holder->title);
  if (image)
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (image), holder->icon);
      gtk_widget_set_visible (image, holder->icon != NULL);
    }
  if (description)
    {
      gtk_label_set_label (GTK_LABEL (description), holder->description);
      gtk_widget_set_visible (description , holder->description != NULL);        
    }
}

static GtkListItemFactory *
strings_factory_new (gboolean full)
{
  GtkListItemFactory *factory;

  factory = gtk_signal_list_item_factory_new ();
  if (full)
    g_signal_connect (factory, "setup", G_CALLBACK (strings_setup_item_full), NULL);
  else
    g_signal_connect (factory, "setup", G_CALLBACK (strings_setup_item_single_line), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (strings_bind_item), NULL);

  return factory;
}

static GListModel *
strings_model_new (const char *const *titles,
                   const char *const *icons,
                   const char *const *descriptions)
{
  GListStore *store;
  int i;

  store = g_list_store_new (STRING_TYPE_HOLDER);
  for (i = 0; titles[i]; i++)
    {
      StringHolder *holder = string_holder_new (titles[i],
                                                icons ? icons[i] : NULL,
                                                descriptions ? descriptions[i] : NULL);
      g_list_store_append (store, holder);
      g_object_unref (holder);
    }

  return G_LIST_MODEL (store);
}

static GtkWidget *
drop_down_new_from_strings (const char *const *titles,
                            const char *const *icons,
                            const char *const *descriptions)
{
  GtkWidget *widget;
  GListModel *model;
  GtkListItemFactory *factory;
  GtkListItemFactory *list_factory;

  g_return_val_if_fail (titles != NULL, NULL);
  g_return_val_if_fail (icons == NULL || g_strv_length ((char **)icons) == g_strv_length ((char **)titles), NULL);
  g_return_val_if_fail (descriptions == NULL || g_strv_length ((char **)icons) == g_strv_length ((char **)descriptions), NULL);

  model = strings_model_new (titles, icons, descriptions);
  factory = strings_factory_new (FALSE);
  if (icons != NULL || descriptions != NULL)
    list_factory = strings_factory_new (TRUE);
  else
    list_factory = NULL;

  widget = g_object_new (GTK_TYPE_DROP_DOWN,
                         "model", model,
                         "factory", factory,
                         "list-factory", list_factory,
                         NULL);

  g_object_unref (model);
  g_object_unref (factory);
  if (list_factory)
    g_object_unref (list_factory);

  return widget;
}

static char *
get_family_name (gpointer item)
{
  return g_strdup (pango_font_family_get_name (PANGO_FONT_FAMILY (item)));
}

static char *
get_title (gpointer item)
{
  return g_strdup (STRING_HOLDER (item)->title);
}

static gboolean
quit_cb (GtkWindow *window,
         gpointer   data)
{
  *((gboolean*)data) = TRUE;

  g_main_context_wakeup (NULL);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *button, *box, *spin, *check;
  GListModel *model;
  GtkExpression *expression;
  const char * const times[] = { "1 minute", "2 minutes", "5 minutes", "20 minutes", NULL };
  const char * const many_times[] = {
    "1 minute", "2 minutes", "5 minutes", "10 minutes", "15 minutes", "20 minutes",
    "25 minutes", "30 minutes", "35 minutes", "40 minutes", "45 minutes", "50 minutes",
    "55 minutes", "1 hour", "2 hours", "3 hours", "5 hours", "6 hours", "7 hours",
    "8 hours", "9 hours", "10 hours", "11 hours", "12 hours", NULL
  };
  const char * const device_titles[] = { "Digital Output", "Headphones", "Digital Output", "Analog Output", NULL };
  const char * const device_icons[] = {  "audio-card-symbolic", "audio-headphones-symbolic", "audio-card-symbolic", "audio-card-symbolic", NULL };
  const char * const device_descriptions[] = {
    "Built-in Audio", "Built-in audio", "Thinkpad Tunderbolt 3 Dock USB Audio", "Thinkpad Tunderbolt 3 Dock USB Audio", NULL
  };
  gboolean quit = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "hello world");
  gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
  g_signal_connect (window, "close-request", G_CALLBACK (quit_cb), &quit);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_margin_start (box, 10);
  gtk_widget_set_margin_end (box, 10);
  gtk_widget_set_margin_top (box, 10);
  gtk_widget_set_margin_bottom (box, 10);
  gtk_window_set_child (GTK_WINDOW (window), box);

  button = gtk_drop_down_new ();

  model = G_LIST_MODEL (pango_cairo_font_map_get_default ());
  gtk_drop_down_set_model (GTK_DROP_DOWN (button), model);
  gtk_drop_down_set_selected (GTK_DROP_DOWN (button), 0);

  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                            0, NULL,
                                            (GCallback)get_family_name,
                                            NULL, NULL);
  gtk_drop_down_set_expression (GTK_DROP_DOWN (button), expression);
  gtk_expression_unref (expression);
  gtk_box_append (GTK_BOX (box), button);

  spin = gtk_spin_button_new_with_range (-1, g_list_model_get_n_items (G_LIST_MODEL (model)), 1);
  gtk_widget_set_halign (spin, GTK_ALIGN_START);
  g_object_bind_property  (button, "selected", spin, "value", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_box_append (GTK_BOX (box), spin);

  check = gtk_check_button_new_with_label ("Enable search");
  g_object_bind_property  (button, "enable-search", check, "active", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_box_append (GTK_BOX (box), check);

  g_object_unref (model);

  button = drop_down_new_from_strings (times, NULL, NULL);
  gtk_box_append (GTK_BOX (box), button);

  button = drop_down_new_from_strings (many_times, NULL, NULL);
  gtk_box_append (GTK_BOX (box), button);

  button = drop_down_new_from_strings (many_times, NULL, NULL);
  gtk_drop_down_set_enable_search (GTK_DROP_DOWN (button), TRUE);
  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                            0, NULL,
                                            (GCallback)get_title,
                                            NULL, NULL);
  gtk_drop_down_set_expression (GTK_DROP_DOWN (button), expression);
  gtk_expression_unref (expression);
  gtk_box_append (GTK_BOX (box), button);

  button = drop_down_new_from_strings (device_titles, device_icons, device_descriptions);
  gtk_box_append (GTK_BOX (box), button);

  gtk_window_present (GTK_WINDOW (window));

  while (!quit)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
