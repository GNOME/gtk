/* Lists/Emoji
 * #Keywords: GtkListItemFactory, GtkGridView
 *
 * This demo uses the GtkGridView widget to show Emoji.
 *
 * It shows how to use sections in GtkGridView
 */

#include <gtk/gtk.h>

static char *
get_section (GtkEmojiObject *object)
{
  switch (gtk_emoji_object_get_group (object))
    {
    case GTK_EMOJI_GROUP_RECENT: return g_strdup ("Recent");
    case GTK_EMOJI_GROUP_SMILEYS: return g_strdup ("Smileys");
    case GTK_EMOJI_GROUP_BODY: return g_strdup ("People");
    case GTK_EMOJI_GROUP_COMPONENT: return g_strdup ("Components");
    case GTK_EMOJI_GROUP_NATURE: return g_strdup ("Nature");
    case GTK_EMOJI_GROUP_FOOD: return g_strdup ("Food");
    case GTK_EMOJI_GROUP_PLACES: return g_strdup ("Places");
    case GTK_EMOJI_GROUP_ACTIVITIES: return g_strdup ("Activities");
    case GTK_EMOJI_GROUP_OBJECTS: return g_strdup ("Objects");
    case GTK_EMOJI_GROUP_SYMBOLS: return g_strdup ("Symbols");
    case GTK_EMOJI_GROUP_FLAGS: return g_strdup ("Flags");
    default: return g_strdup ("Something else");
    }
}

static void
setup_section_listitem_cb (GtkListItemFactory *factory,
                           GtkListItem        *list_item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_add_css_class (label, "heading");
  gtk_widget_set_margin_top (label, 4);
  gtk_widget_set_margin_bottom (label, 4);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_section_listitem_cb (GtkListItemFactory *factory,
                          GtkListItem        *list_item)
{
  GtkWidget *label;
  GtkEmojiObject *item;
  char *text;

  label = gtk_list_item_get_child (list_item);
  item = gtk_list_item_get_item (list_item);

  text = get_section (item);
  gtk_label_set_label (GTK_LABEL (label), text);
  g_free (text);
}

static void
setup_listitem_cb (GtkListItemFactory *factory,
                   GtkListItem        *list_item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_listitem_cb (GtkListItemFactory *factory,
                  GtkListItem        *list_item)
{
  GtkWidget *label;
  GtkEmojiObject *item;
  char buffer[64];
  PangoAttrList *attrs;

  label = gtk_list_item_get_child (list_item);
  item = gtk_list_item_get_item (list_item);

  gtk_emoji_object_get_text (item, buffer, sizeof (buffer), 0);
  gtk_label_set_label (GTK_LABEL (label), buffer);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);
}

static gboolean
match_tokens (const char **term_tokens,
              const char **hit_tokens)
{
  int i, j;
  gboolean matched;

  matched = TRUE;

  for (i = 0; term_tokens[i]; i++)
    {
      for (j = 0; hit_tokens[j]; j++)
        if (g_str_has_prefix (hit_tokens[j], term_tokens[i]))
          goto one_matched;

      matched = FALSE;
      break;

one_matched:
      continue;
    }

  return matched;
}

static gboolean
filter_func (gpointer item,
             gpointer user_data)
{
  GtkEmojiObject *emoji = item;
  GtkWidget *entry = user_data;
  const char *text;
  const char *name;
  const char **keywords;
  char **term_tokens;
  char **name_tokens;
  gboolean res;

  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  if (text[0] == 0)
    return TRUE;

  name = gtk_emoji_object_get_name (emoji);
  keywords = gtk_emoji_object_get_keywords (emoji);

  term_tokens = g_str_tokenize_and_fold (text, "en", NULL);
  name_tokens = g_str_tokenize_and_fold (name, "en", NULL);

  res = match_tokens ((const char **)term_tokens, (const char **)name_tokens) ||
        match_tokens ((const char **)term_tokens, keywords);

  g_strfreev (term_tokens);
  g_strfreev (name_tokens);

  return res;
}

static void
search_changed (GtkEntry *entry,
                gpointer  data)
{
  GtkFilter *filter = data;

  gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static GtkWidget *window = NULL;

GtkWidget *
do_listview_emoji (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkWidget *list, *sw;
      GListModel *model;
      GtkListItemFactory *factory;
      GtkWidget *box, *entry;
      GtkFilter *filter;

      /* Create a window and set a few defaults */
      window = gtk_window_new ();
      gtk_window_set_default_size (GTK_WINDOW (window), 300, 400);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Emoji");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_listitem_cb), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_listitem_cb), NULL);

      entry = gtk_search_entry_new ();

      model = G_LIST_MODEL (gtk_emoji_list_new ());
      filter = GTK_FILTER (gtk_custom_filter_new (filter_func, entry, NULL));
      model = G_LIST_MODEL (gtk_filter_list_model_new (model, filter));

      g_signal_connect (entry, "search-changed", G_CALLBACK (search_changed), filter);

      list = gtk_grid_view_new (GTK_SELECTION_MODEL (gtk_no_selection_new (model)), factory);
      gtk_grid_view_set_max_columns (GTK_GRID_VIEW (list), 20);

      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_section_listitem_cb), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_section_listitem_cb), NULL);
      gtk_grid_view_set_section_factory (GTK_GRID_VIEW (list), factory);
      g_object_unref (factory);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_append (GTK_BOX (box), entry);
      sw = gtk_scrolled_window_new ();
      gtk_widget_set_hexpand (sw, TRUE);
      gtk_widget_set_vexpand (sw, TRUE);
      gtk_box_append (GTK_BOX (box), sw);
      gtk_window_set_child (GTK_WINDOW (window), box);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
