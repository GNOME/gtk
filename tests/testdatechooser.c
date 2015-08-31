#include <gtk/gtk.h>

static const gchar css[] =
".weekday {"
"  background: darkgray;"
"  padding-left: 6px;"
"  padding-right: 6px;"
"}"
".weeknum {"
"  background: darkgray;"
"  padding-top: 6px;"
"  padding-bottom: 6px;"
"}"
".day {"
"  margin: 6px;"
"}"
".day.other-month {"
"  color: gray;"
"}"
".day:selected {"
"  background: @theme_selected_bg_color;"
"}";

int
main (int argc, char *argv[])
{
  GtkWidget *window, *calendar;
  GtkCssProvider *provider;
  GError *error = NULL;

  gtk_init (NULL, NULL);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, sizeof (css), &error);
  if (error)
    {
      g_print ("%s", error->message);
      g_error_free (error);
    }
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider), 800);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  calendar = gtk_date_chooser_widget_new ();

  gtk_container_add (GTK_CONTAINER (window), calendar);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
