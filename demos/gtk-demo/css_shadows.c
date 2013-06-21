/* CSS Theming/Shadows
 *
 * This demo shows how to use CSS shadows.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static void
show_parsing_error (GtkCssProvider *provider,
                    GtkCssSection  *section,
                    const GError   *error,
                    GtkTextBuffer  *buffer)
{
  GtkTextIter start, end;
  const char *tag_name;

  gtk_text_buffer_get_iter_at_line_index (buffer,
                                          &start,
                                          gtk_css_section_get_start_line (section),
                                          gtk_css_section_get_start_position (section));
  gtk_text_buffer_get_iter_at_line_index (buffer,
                                          &end,
                                          gtk_css_section_get_end_line (section),
                                          gtk_css_section_get_end_position (section));

  if (g_error_matches (error, GTK_CSS_PROVIDER_ERROR, GTK_CSS_PROVIDER_ERROR_DEPRECATED))
    tag_name = "warning";
  else
    tag_name = "error";

  gtk_text_buffer_apply_tag_by_name (buffer, tag_name, &start, &end);
}
                    
static void
css_text_changed (GtkTextBuffer  *buffer,
                  GtkCssProvider *provider)
{
  GtkTextIter start, end;
  char *text;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_remove_all_tags (buffer, &start, &end);

  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  gtk_css_provider_load_from_data (provider, text, -1, NULL);
  g_free (text);

  gtk_style_context_reset_widgets (gdk_screen_get_default ());
}

static void
apply_css (GtkWidget *widget, GtkStyleProvider *provider)
{
  gtk_style_context_add_provider (gtk_widget_get_style_context (widget), provider, G_MAXUINT);
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), (GtkCallback) apply_css, provider);
}

GtkWidget *
create_toolbar (void)
{
  GtkWidget *toolbar;
  GtkToolItem *item;

  toolbar = gtk_toolbar_new ();
  gtk_widget_set_valign (toolbar, GTK_ALIGN_CENTER);

  item = gtk_tool_button_new (NULL, NULL);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "go-next");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  item = gtk_tool_button_new (NULL, NULL);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "go-previous");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  item = gtk_tool_button_new (NULL, "Hello World");
  gtk_tool_item_set_is_important (item, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  return toolbar;
}

GtkWidget *
do_css_shadows (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *paned, *container, *child;
      GtkStyleProvider *provider;
      GtkTextBuffer *text;
      GBytes *bytes;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      paned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
      gtk_container_add (GTK_CONTAINER (window), paned);

      child = create_toolbar ();
      gtk_container_add (GTK_CONTAINER (paned), child);

      text = gtk_text_buffer_new (NULL);
      gtk_text_buffer_create_tag (text,
                                  "warning",
                                  "underline", PANGO_UNDERLINE_SINGLE,
                                  NULL);
      gtk_text_buffer_create_tag (text,
                                  "error",
                                  "underline", PANGO_UNDERLINE_ERROR,
                                  NULL);

      provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
      
      container = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (paned), container);
      child = gtk_text_view_new_with_buffer (text);
      gtk_container_add (GTK_CONTAINER (container), child);
      g_signal_connect (text,
                        "changed",
                        G_CALLBACK (css_text_changed),
                        provider);

      bytes = g_resources_lookup_data ("/css_shadows/gtk.css", 0, NULL);
      gtk_text_buffer_set_text (text, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes));
      g_bytes_unref (bytes);

      g_signal_connect (provider,
                        "parsing-error",
                        G_CALLBACK (show_parsing_error),
                        gtk_text_view_get_buffer (GTK_TEXT_VIEW (child)));

      apply_css (window, provider);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
