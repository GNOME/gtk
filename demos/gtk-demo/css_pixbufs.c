/* Theming/Animated Backgrounds
 *
 * This demo is done in honour of the Pixbufs demo further down.
 * It is done exclusively with CSS as the background of the window.
 */

#include <gtk/gtk.h>

static void
show_parsing_error (GtkCssStyleSheet *stylesheet,
                    GtkCssSection  *section,
                    const GError   *error,
                    GtkTextBuffer  *buffer)
{
  const GtkCssLocation *start_location, *end_location;
  GtkTextIter start, end;
  const char *tag_name;

  start_location = gtk_css_section_get_start_location (section);
  gtk_text_buffer_get_iter_at_line_index (buffer,
                                          &start,
                                          start_location->lines,
                                          start_location->line_bytes);
  end_location = gtk_css_section_get_end_location (section);
  gtk_text_buffer_get_iter_at_line_index (buffer,
                                          &end,
                                          end_location->lines,
                                          end_location->line_bytes);


  if (error->domain == GTK_CSS_PARSER_WARNING)
    tag_name = "warning";
  else
    tag_name = "error";

  gtk_text_buffer_apply_tag_by_name (buffer, tag_name, &start, &end);
}

static void
css_text_changed (GtkTextBuffer  *buffer,
                  GtkCssStyleSheet *stylesheet)
{
  GtkTextIter start, end;
  char *text;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_remove_all_tags (buffer, &start, &end);

  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  gtk_css_style_sheet_load_from_data (stylesheet, text, -1);
  g_free (text);
}

static void
apply_css (GtkWidget *widget, GtkStyleProvider *stylesheet)
{
  gtk_style_context_add_provider (gtk_widget_get_style_context (widget), stylesheet, G_MAXUINT);
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), (GtkCallback) apply_css, stylesheet);
}

GtkWidget *
do_css_pixbufs (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *paned, *container, *child;
      GtkStyleProvider *stylesheet;
      GtkTextBuffer *text;
      GBytes *bytes;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Animated Backgrounds");
      gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      paned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
      gtk_container_add (GTK_CONTAINER (window), paned);

      /* Need a filler so we get a handle */
      child = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
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

      stylesheet = GTK_STYLE_PROVIDER (gtk_css_style_sheet_new ());

      container = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (paned), container);
      child = gtk_text_view_new_with_buffer (text);
      gtk_container_add (GTK_CONTAINER (container), child);
      g_signal_connect (text, "changed",
                        G_CALLBACK (css_text_changed), stylesheet);

      bytes = g_resources_lookup_data ("/css_pixbufs/gtk.css", 0, NULL);
      gtk_text_buffer_set_text (text, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes));
      g_bytes_unref (bytes);

      g_signal_connect (stylesheet,
                        "parsing-error",
                        G_CALLBACK (show_parsing_error),
                        gtk_text_view_get_buffer (GTK_TEXT_VIEW (child)));

      apply_css (window, stylesheet);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
