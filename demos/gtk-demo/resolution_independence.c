/* Resolution Independence
 *
 * This demonstates resolution independence features available since
 * version 2.14 of GTK+. Use the slider to change the effective DPI
 * per monitor. Note that the changes will only affect windows from
 * this process.
 */
#include <gtk/gtk.h>
#include "config.h"
#include "demo-common.h"

static GtkWidget *window = NULL;
static GtkWindowGroup *window_group = NULL;
static GtkWidget *hscale;
static GtkWidget *label;
static GtkWidget *button;
static gdouble button_dpi;

#define INCHES_TO_MM 25.4

#define MIN_DPI 24.0
#define MAX_DPI 480.0
#define STEP_DPI 1.0

static void
update (void)
{
  GdkScreen *screen;
  gint monitor_num;
  char *plug_name;
  char *s;
  gdouble dpi;
  char *font_name;
  GtkSettings *settings;
  int width_mm;
  int height_mm;
  GdkRectangle geometry;

  plug_name = NULL;
  font_name = NULL;
  width_mm = -1;
  height_mm = -1;
  geometry.x = -1;
  geometry.y = -1;
  geometry.width = -1;
  geometry.height = -1;

  screen = gtk_window_get_screen (GTK_WINDOW (window));
  monitor_num = gtk_widget_get_monitor_num (window);
  if (screen != NULL && monitor_num >= 0)
    {
      plug_name = gdk_screen_get_monitor_plug_name (screen, monitor_num);
      width_mm = gdk_screen_get_monitor_width_mm (screen, monitor_num);
      height_mm = gdk_screen_get_monitor_height_mm (screen, monitor_num);
      gdk_screen_get_monitor_geometry (screen, monitor_num, &geometry);
    }
  if (screen != NULL)
    settings = gtk_settings_get_for_screen (screen);
  else
    settings = gtk_settings_get_default ();
  g_object_get (settings, "gtk-font-name", &font_name, NULL);

  s = g_strdup_printf ("Monitor %d (%s) @ %dx%d+%d+%d\n"
                       "%d mm x %d mm\n"
		       "DPI: %.1f x %.1f\n"
                       "Font \"%s\"\n"
                       "1 em -> %g pixels\n"
                       "1 mm -> %g pixels",
                       monitor_num,
                       plug_name != NULL ? plug_name : "unknown name",
                       geometry.width, geometry.height, geometry.x, geometry.y,
                       width_mm, height_mm,
		       INCHES_TO_MM * geometry.width / width_mm,
		       INCHES_TO_MM * geometry.height / height_mm,
                       font_name,
                       gtk_size_to_pixel_double (screen, monitor_num, gtk_size_em (1.0)),
                       gtk_size_to_pixel_double (screen, monitor_num, gtk_size_mm (1.0)));
  gtk_label_set_text (GTK_LABEL (label), s);
  g_free (s);

  button_dpi = MIN (INCHES_TO_MM * geometry.width / width_mm,
                    INCHES_TO_MM * geometry.height / height_mm);
  s = g_strdup_printf ("Set DPI to %.1f", button_dpi);
  gtk_button_set_label (GTK_BUTTON (button), s);
  g_free (s);

  dpi = -1;
  if (screen != NULL)
    {
      dpi = gdk_screen_get_resolution_for_monitor (screen, monitor_num);
      gtk_range_set_value (GTK_RANGE (hscale), dpi);
    }

  g_free (plug_name);
  g_free (font_name);
}

static void
window_mapped (GtkWidget *widget, gpointer user_data)
{
  update ();
}

static void
unit_changed (GtkWidget *widget, gpointer user_data)
{
  update ();
}

static void
monitor_num_notify (GtkWindow *window, GParamSpec *psec, gpointer user_data)
{
  g_debug ("notify::monitor-num");
  update ();
}

static void
update_value (void)
{
  gdouble slider_value;

  slider_value = gtk_range_get_value (GTK_RANGE (hscale));
  if (slider_value >= MIN_DPI && slider_value <= MAX_DPI)
    {
      GdkScreen *screen;
      gint monitor_num;

      screen = gtk_window_get_screen (GTK_WINDOW (window));
      monitor_num = gtk_widget_get_monitor_num (window);
      if (screen != NULL && monitor_num >= 0)
        {
          gdk_screen_set_resolution_for_monitor (screen, monitor_num, slider_value);
          g_signal_emit_by_name (screen, "monitors-changed");
        }
    }
}

static gboolean is_pressed = FALSE;

static gboolean
hscale_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  is_pressed = TRUE;
  return FALSE;
}

static gboolean
hscale_button_release_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  is_pressed = FALSE;
  update_value ();
  return FALSE;
}

static void
hscale_value_changed (GtkRange *range, gpointer user_data)
{
  if (!is_pressed)
    update_value ();
}

static char *
hscale_format_value (GtkScale *scale, gdouble value, gpointer user_data)
{
  return g_strdup_printf ("%g DPI", value);
}

static void
dpi_button_clicked (GtkButton *button, gpointer user_data)
{
  gtk_range_set_value (GTK_RANGE (hscale), button_dpi);
}

GtkWidget *
do_resolution_independence (GtkWidget *do_widget)
{
  GtkWidget *vbox;

  if (window != NULL)
    goto have_window;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_screen (GTK_WINDOW (window), gtk_widget_get_screen (do_widget));
  gtk_window_set_title (GTK_WINDOW (window), "Resolution Independence");
  gtk_window_set_icon_name (GTK_WINDOW (window), "gtk-fullscreen");

  g_signal_connect (G_OBJECT (window), "map", G_CALLBACK (window_mapped), NULL);
  g_signal_connect (G_OBJECT (window), "notify::monitor-num", G_CALLBACK (monitor_num_notify), NULL);
  g_signal_connect (G_OBJECT (window), "unit-changed", G_CALLBACK (unit_changed), NULL);

  g_signal_connect (window, "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &window);

  vbox = gtk_vbox_new (FALSE, gtk_size_em (1));

  label = gtk_label_new (NULL);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  hscale = gtk_hscale_new_with_range (MIN_DPI, MAX_DPI, STEP_DPI);
  gtk_box_pack_start (GTK_BOX (vbox), hscale, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (hscale), "value-changed", G_CALLBACK (hscale_value_changed), NULL);
  g_signal_connect (G_OBJECT (hscale), "button-press-event", G_CALLBACK (hscale_button_press_event), NULL);
  g_signal_connect (G_OBJECT (hscale), "button-release-event", G_CALLBACK (hscale_button_release_event), NULL);
  g_signal_connect (G_OBJECT (hscale), "format-value", G_CALLBACK (hscale_format_value), NULL);

  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (window), gtk_size_em (1));
  gtk_widget_set_size_request (window, GTK_SIZE_ONE_TWELFTH_EM (500), -1);
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  button = gtk_button_new ();
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (dpi_button_clicked), NULL);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  /* avoid dialogs (e.g. printing) grabbing focus from us */
  if (window_group == NULL)
    window_group = gtk_window_group_new ();
  gtk_window_group_add_window (window_group, GTK_WINDOW (window));

  /* make sure we're on top */
  gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);

 have_window:

  if (!GTK_WIDGET_VISIBLE (window))
    {
      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
