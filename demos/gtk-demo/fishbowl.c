/* Benchmark/Fishbowl
 *
 * This demo models the fishbowl demos seen on the web in a GTK way.
 * It's also a neat little tool to see how fast your computer (or
 * your GTK version) is.
 */

#include <gtk/gtk.h>

#include "gtkfishbowl.h"
#include "gtkgears.h"

const char *const css =
".blurred-button {"
"  box-shadow: 0px 0px 5px 10px rgba(0, 0, 0, 0.5);"
"}"
"";

char **icon_names = NULL;
gsize n_icon_names = 0;

static void
init_icon_names (GtkIconTheme *theme)
{
  GPtrArray *icons;
  GList *l, *icon_list;

  if (icon_names)
    return;

  icon_list = gtk_icon_theme_list_icons (theme, NULL);
  icons = g_ptr_array_new ();

  for (l = icon_list; l; l = l->next)
    {
      if (g_str_has_suffix (l->data, "symbolic"))
        continue;

      g_ptr_array_add (icons, g_strdup (l->data));
    }

  n_icon_names = icons->len;
  g_ptr_array_add (icons, NULL); /* NULL-terminate the array */
  icon_names = (char **) g_ptr_array_free (icons, FALSE);

  /* don't free strings, we assigned them to the array */
  g_list_free_full (icon_list, g_free);
}

static const char *
get_random_icon_name (GtkIconTheme *theme)
{
  init_icon_names (theme);

  return icon_names[g_random_int_range(0, n_icon_names)];
}

GtkWidget *
create_icon (void)
{
  GtkWidget *image;

  image = gtk_image_new_from_icon_name (get_random_icon_name (gtk_icon_theme_get_default ()));
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);

  return image;
}

static GtkWidget *
create_button (void)
{
  return gtk_button_new_with_label ("Button");
}

static GtkWidget *
create_blurred_button (void)
{
  GtkWidget *w = gtk_button_new ();

  gtk_style_context_add_class (gtk_widget_get_style_context (w), "blurred-button");

  return w;
}

static GtkWidget *
create_font_button (void)
{
  return gtk_font_button_new ();
}

static GtkWidget *
create_level_bar (void)
{
  GtkWidget *w = gtk_level_bar_new_for_interval (0, 100);

  gtk_level_bar_set_value (GTK_LEVEL_BAR (w), 50);

  /* Force them to be a bit larger */
  gtk_widget_set_size_request (w, 200, -1);

  return w;
}

static GtkWidget *
create_spinner (void)
{
  GtkWidget *w = gtk_spinner_new ();

  gtk_spinner_start (GTK_SPINNER (w));

  return w;
}

static GtkWidget *
create_spinbutton (void)
{
  GtkWidget *w = gtk_spin_button_new_with_range (0, 10, 1);

  return w;
}

static GtkWidget *
create_label (void)
{
  GtkWidget *w = gtk_label_new ("pLorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua.");

  gtk_label_set_wrap (GTK_LABEL (w), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (w), 100);

  return w;
}

static GtkWidget *
create_video (void)
{
  GtkMediaStream *stream = GTK_MEDIA_STREAM (gtk_media_file_new_for_resource ("/images/gtk-logo.webm"));
  GtkWidget *w = gtk_picture_new_for_paintable (GDK_PAINTABLE (stream));

  gtk_widget_set_size_request (w, 64, 64);
  gtk_media_stream_set_loop (stream, TRUE);
  gtk_media_stream_play (stream);
  g_object_unref (stream);

  return w;
}

static GtkWidget *
create_gears (void)
{
  GtkWidget *w = gtk_gears_new ();

  gtk_widget_set_size_request (w, 100, 100);

  return w;
}

static GtkWidget *
create_switch (void)
{
  GtkWidget *w = gtk_switch_new ();

  gtk_switch_set_state (GTK_SWITCH (w), TRUE);

  return w;
}

static void
mapped (GtkWidget *w)
{
  gtk_menu_button_popup (GTK_MENU_BUTTON (w));
}

static GtkWidget *
create_menu_button (void)
{
  GtkWidget *w = gtk_menu_button_new ();
  GtkWidget *popover = gtk_popover_new (NULL);

  gtk_container_add (GTK_CONTAINER (popover), gtk_button_new_with_label ("Hey!"));
  gtk_popover_set_autohide (GTK_POPOVER (popover), FALSE);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (w), popover);
  g_signal_connect (w, "map", G_CALLBACK (mapped), NULL);

  return w;
}

static const struct {
  const char *name;
  GtkWidget * (*create_func) (void);
} widget_types[] = {
  { "Icon",       create_icon           },
  { "Button",     create_button         },
  { "Blurbutton", create_blurred_button },
  { "Fontbutton", create_font_button    },
  { "Levelbar",   create_level_bar      },
  { "Label",      create_label          },
  { "Spinner",    create_spinner        },
  { "Spinbutton", create_spinbutton     },
  { "Video",      create_video          },
  { "Gears",      create_gears          },
  { "Switch",     create_switch         },
  { "Menubutton", create_menu_button    },
};

static int selected_widget_type = -1;
static const int N_WIDGET_TYPES = G_N_ELEMENTS (widget_types);

static void
set_widget_type (GtkFishbowl *fishbowl,
                 int          widget_type_index)
{
  GtkWidget *window, *headerbar;

  if (widget_type_index == selected_widget_type)
    return;

  selected_widget_type = widget_type_index;

  gtk_fishbowl_set_creation_func (fishbowl,
                                  widget_types[selected_widget_type].create_func);

  window = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (fishbowl)));
  headerbar = gtk_window_get_titlebar (GTK_WINDOW (window));
  gtk_header_bar_set_title (GTK_HEADER_BAR (headerbar),
                            widget_types[selected_widget_type].name);
}

void
next_button_clicked_cb (GtkButton *source,
                        gpointer   user_data)
{
  GtkFishbowl *fishbowl = user_data;
  int new_index;

  if (selected_widget_type + 1 >= N_WIDGET_TYPES)
    new_index = 0;
  else
    new_index = selected_widget_type + 1;

  set_widget_type (fishbowl, new_index);
}

void
prev_button_clicked_cb (GtkButton *source,
                        gpointer   user_data)
{
  GtkFishbowl *fishbowl = user_data;
  int new_index;

  if (selected_widget_type - 1 < 0)
    new_index = N_WIDGET_TYPES - 1;
  else
    new_index = selected_widget_type - 1;

  set_widget_type (fishbowl, new_index);
}

void
changes_toggled_cb (GtkToggleButton *button,
                    gpointer         user_data)
{
  if (gtk_toggle_button_get_active (button))
    gtk_button_set_icon_name (GTK_BUTTON (button), "changes-prevent");
  else
    gtk_button_set_icon_name (GTK_BUTTON (button), "changes-allow");
}

GtkWidget *
do_fishbowl (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  static GtkCssProvider *provider = NULL;

  if (provider == NULL)
    {
      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_data (provider, css, -1);
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  if (!window)
    {
      GtkBuilder *builder;
      GtkWidget *bowl;

      g_type_ensure (GTK_TYPE_FISHBOWL);

      builder = gtk_builder_new_from_resource ("/fishbowl/fishbowl.ui");
      gtk_builder_add_callback_symbols (builder,
                                        "next_button_clicked_cb", G_CALLBACK (next_button_clicked_cb),
                                        "prev_button_clicked_cb", G_CALLBACK (prev_button_clicked_cb),
                                        "changes_toggled_cb", G_CALLBACK (changes_toggled_cb),
                                        NULL);
      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      bowl = GTK_WIDGET (gtk_builder_get_object (builder, "bowl"));
      selected_widget_type = -1;
      set_widget_type (GTK_FISHBOWL (bowl), 0);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_widget_realize (window);
      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
