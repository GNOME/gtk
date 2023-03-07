#include "gtkcssprovider.h"
#include <gtk/gtk.h>

static GtkRequisition size;
static gint64 start_time;
static gboolean stop_update_size;

static gboolean
update_size (GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
  GdkDrag *drag = data;
  gint64 now = g_get_monotonic_time ();
  float t;
  int width, height;

  if (stop_update_size)
    return G_SOURCE_REMOVE;

  t = fmodf ((now - start_time) / (float) G_TIME_SPAN_SECOND, 1);
  if (t >= 0.5)
    t = 1 - t;

  width = size.width + t * 300;
  height = size.height + t * 150;

  gtk_widget_set_size_request (widget, width, height);
  gdk_drag_set_hotspot (drag, width / 2, height / 2);

  return G_SOURCE_CONTINUE;
}

static void
drag_begin (GtkDragSource *source,
            GdkDrag       *drag)
{
  GtkWidget *widget;
  GtkWidget *icon;

  icon = gtk_drag_icon_get_for_drag (drag);

  widget = gtk_label_new ("This Should Resize\n\nAnd Stay Centered");
  gtk_widget_add_css_class (widget, "dnd");
  gtk_widget_get_preferred_size (widget, NULL, &size);

  gtk_drag_icon_set_child (GTK_DRAG_ICON (icon), widget);
  gtk_widget_set_size_request (widget, size.width, size.height);
  gdk_drag_set_hotspot (drag, size.width / 2, size.height / 2);

  start_time = g_get_monotonic_time ();
  stop_update_size = FALSE;
  gtk_widget_add_tick_callback (widget, update_size, drag, NULL);
}

static void
drag_end (GtkDragSource *source,
          GdkDrag       *drag,
          gboolean       delete_data,
          gboolean       data)
{
  stop_update_size = TRUE;
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GtkCssProvider *provider;
  GtkWidget *window;
  GtkWidget *label;
  GtkDragSource *source;
  GdkContentProvider *content;
  gboolean done = FALSE;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider,
                                   ".dnd {"
                                   "background-color: red;"
                                   "border-top: 10px solid rebeccapurple;"
                                   "}",
                                   -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  label = gtk_label_new ("Drag Me");
  g_object_set (label,
                "margin-start", 64,
                "margin-end", 64,
                "margin-top", 64,
                "margin-bottom", 64,
                NULL);

  source = gtk_drag_source_new ();
  content = gdk_content_provider_new_typed (G_TYPE_STRING, "I'm data!");
  gtk_drag_source_set_content (source, content);
  g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), NULL);
  g_signal_connect (source, "drag-end", G_CALLBACK (drag_end), NULL);
  gtk_widget_add_controller (label, GTK_EVENT_CONTROLLER (source));

  gtk_window_set_child (GTK_WINDOW (window), label);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
