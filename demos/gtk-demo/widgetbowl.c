/* Benchmark/Widgetbowl
 *
 * This is a version of the Fishbowl demo that instead shows different
 * kinds of widgets, which is useful for comparing the rendering performance
 * of theme specifics.
 */

#include <gtk/gtk.h>

#include "gtkfishbowl.h"
#include "gtkgears.h"

const char *const css =
".blurred-button {"
"  box-shadow: 0px 0px 5px 10px rgba(0, 0, 0, 0.5);"
"}"
"";

GtkWidget *fishbowl;

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

  gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (w), 100);

  return w;
}

static GtkWidget *
create_video (void)
{
  GtkMediaStream *stream = gtk_media_file_new_for_resource ("/images/gtk-logo.webm");
  GtkWidget *w = gtk_image_new_from_paintable (GDK_PAINTABLE (stream));
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

static const struct {
  const char *name;
  GtkWidget * (*create_func) (void);
} widget_types[] = {
  { "Button",     create_button         },
  { "Blurbutton", create_blurred_button },
  { "Fontbutton", create_font_button    },
  { "Levelbar"  , create_level_bar      },
  { "Label"     , create_label          },
  { "Spinner"   , create_spinner        },
  { "Spinbutton", create_spinbutton     },
  { "Video",      create_video          },
  { "Gears",      create_gears          },
  { "Switch",     create_switch         },
};

static int selected_widget_type = -1;
static const int N_WIDGET_TYPES = G_N_ELEMENTS (widget_types);

#define N_STATS 5

#define STATS_UPDATE_TIME G_USEC_PER_SEC

static void
set_widget_type (GtkWidget *headerbar,
                 int        widget_type_index)
{
  if (widget_type_index == selected_widget_type)
    return;

  selected_widget_type = widget_type_index;

  gtk_header_bar_set_title (GTK_HEADER_BAR (headerbar),
                            widget_types[selected_widget_type].name);
  gtk_fishbowl_set_creation_func (GTK_FISHBOWL (fishbowl),
                                  widget_types[selected_widget_type].create_func);
}

static void
next_button_clicked_cb (GtkButton *source,
                        gpointer   user_data)
{
  GtkWidget *headerbar = user_data;
  int new_index;

  if (selected_widget_type + 1 >= N_WIDGET_TYPES)
    new_index = 0;
  else
    new_index = selected_widget_type + 1;

  set_widget_type (headerbar, new_index);
}

static void
prev_button_clicked_cb (GtkButton *source,
                        gpointer   user_data)
{
  GtkWidget *headerbar = user_data;
  int new_index;

  if (selected_widget_type - 1 < 0)
    new_index = N_WIDGET_TYPES - 1;
  else
    new_index = selected_widget_type - 1;

  set_widget_type (headerbar, new_index);
}

GtkWidget *
do_widgetbowl (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  static GtkCssProvider *provider = NULL;

  gtk_init ();

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
      GtkWidget *info_label;
      GtkWidget *count_label;
      GtkWidget *titlebar;
      GtkWidget *title_box;
      GtkWidget *left_box;
      GtkWidget *next_button;
      GtkWidget *prev_button;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      titlebar = gtk_header_bar_new ();
      gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (titlebar), TRUE);
      info_label = gtk_label_new ("widget - 00.0 fps");
      count_label = gtk_label_new ("0");
      fishbowl = gtk_fishbowl_new ();
      title_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      prev_button = gtk_button_new_from_icon_name ("pan-start-symbolic");
      next_button = gtk_button_new_from_icon_name ("pan-end-symbolic");
      left_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

      g_object_bind_property (fishbowl, "count", count_label, "label", 0);
      g_signal_connect (next_button, "clicked", G_CALLBACK (next_button_clicked_cb), titlebar);
      g_signal_connect (prev_button, "clicked", G_CALLBACK (prev_button_clicked_cb), titlebar);

      gtk_fishbowl_set_animating (GTK_FISHBOWL (fishbowl), TRUE);
      gtk_fishbowl_set_benchmark (GTK_FISHBOWL (fishbowl), TRUE);

      gtk_widget_set_hexpand (title_box, TRUE);
      gtk_widget_set_halign (title_box, GTK_ALIGN_END);

      gtk_window_set_titlebar (GTK_WINDOW (window), titlebar);
      gtk_container_add (GTK_CONTAINER (title_box), count_label);
      gtk_container_add (GTK_CONTAINER (title_box), info_label);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (titlebar), title_box);
      gtk_container_add (GTK_CONTAINER (window), fishbowl);


      gtk_style_context_add_class (gtk_widget_get_style_context (left_box), "linked");
      gtk_container_add (GTK_CONTAINER (left_box), prev_button);
      gtk_container_add (GTK_CONTAINER (left_box), next_button);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (titlebar), left_box);

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_widget_realize (window);

      set_widget_type (titlebar, 0);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);


  return window;
}
