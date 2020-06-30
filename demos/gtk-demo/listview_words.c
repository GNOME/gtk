/* Lists/Words
 *
 * This demo shows filtering a long list - of words.
 *
 * You should have the file `/usr/share/dict/words` installed for
 * this demo to work.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;
static GtkWidget *progress;

const char *factory_text =
"<?xml version='1.0' encoding='UTF-8'?>\n"
"<interface>\n"
"  <template class='GtkListItem'>\n"
"    <property name='child'>\n"
"      <object class='GtkLabel'>\n"
"        <binding name='label'>\n"
"          <lookup name='string' type='GtkStringObject'>\n"
"            <lookup name='item'>GtkListItem</lookup>\n"
"          </lookup>\n"
"        </binding>\n"
"      </object>\n"
"    </property>\n"
"  </template>\n"
"</interface>\n";

static void
update_title_cb (GtkFilterListModel *model)
{
  guint total;
  char *title;
  guint pending;

  total = g_list_model_get_n_items (gtk_filter_list_model_get_model (model));
  pending = gtk_filter_list_model_get_pending (model);

  title = g_strdup_printf ("%u Words", g_list_model_get_n_items (G_LIST_MODEL (model)));

  gtk_widget_set_visible (progress, pending != 0);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress), (total - pending) / (double) total);
  gtk_window_set_title (GTK_WINDOW (window), title);
  g_free (title);
}

GtkWidget *
do_listview_words (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkWidget *header, *listview, *sw, *vbox, *search_entry;
      GtkWidget *overlay;
      GtkFilterListModel *filter_model;
      GtkNoSelection *selection;
      GtkStringList *stringlist;
      GtkFilter *filter;
      char **words;
      char *usr_dict_words;
      GtkExpression *expression;

      if (g_file_get_contents ("/usr/share/dict/words", &usr_dict_words, NULL, NULL))
        {
          words = g_strsplit (usr_dict_words, "\n", -1);
          g_free (usr_dict_words);
        }
      else
        {
          words = g_strsplit ("lorem ipsum dolor sit amet consectetur adipisci elit sed eiusmod tempor incidunt labore et dolore magna aliqua ut enim ad minim veniam quis nostrud exercitation ullamco laboris nisi ut aliquid ex ea commodi consequat", " ", -1);
        }
      stringlist = gtk_string_list_new ((const char **) words);
      g_strfreev (words);

      filter = gtk_string_filter_new ();
      expression = gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string");
      gtk_string_filter_set_expression (GTK_STRING_FILTER (filter), expression);
      gtk_expression_unref (expression);
      filter_model = gtk_filter_list_model_new (G_LIST_MODEL (stringlist), filter);
      gtk_filter_list_model_set_incremental (filter_model, TRUE);

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Words");
      header = gtk_header_bar_new ();
      gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
      gtk_window_set_titlebar (GTK_WINDOW (window), header);
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 600);

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer*)&window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), vbox);

      search_entry = gtk_search_entry_new ();
      g_object_bind_property (search_entry, "text", filter, "search", 0);
      gtk_box_append (GTK_BOX (vbox), search_entry);

      overlay = gtk_overlay_new ();
      gtk_box_append (GTK_BOX (vbox), overlay);

      progress = gtk_progress_bar_new ();
      gtk_widget_set_halign (progress, GTK_ALIGN_FILL);
      gtk_widget_set_valign (progress, GTK_ALIGN_START);
      gtk_widget_set_hexpand (progress, TRUE);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), progress);

      sw = gtk_scrolled_window_new ();
      gtk_overlay_set_child (GTK_OVERLAY (overlay), sw);

      listview = gtk_list_view_new_with_factory (
          gtk_builder_list_item_factory_new_from_bytes (NULL,
              g_bytes_new_static (factory_text, strlen (factory_text))));
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), listview);
      selection = gtk_no_selection_new (G_LIST_MODEL (filter_model));
      gtk_list_view_set_model (GTK_LIST_VIEW (listview), G_LIST_MODEL (selection));
      g_object_unref (selection);

      g_signal_connect (filter_model, "items-changed", G_CALLBACK (update_title_cb), progress);
      g_signal_connect (filter_model, "notify::pending", G_CALLBACK (update_title_cb), progress);
      update_title_cb (filter_model);

      g_object_unref (filter_model);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
