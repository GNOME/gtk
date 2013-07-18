
#include <gtk/gtk.h>
#include "widgets.h"

typedef enum {
  SNAPSHOT_WINDOW,
  SNAPSHOT_DRAW
} SnapshotMode;

static gboolean
quit_when_idle (gpointer loop)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static void
check_for_draw (GdkEvent *event, gpointer loop)
{
  if (event->type == GDK_EXPOSE)
    {
      g_idle_add (quit_when_idle, loop);
      gdk_event_handler_set ((GdkEventFunc) gtk_main_do_event, NULL, NULL);
    }

  gtk_main_do_event (event);
}

static cairo_surface_t *
snapshot_widget (GtkWidget *widget, SnapshotMode mode)
{
  cairo_surface_t *surface;
  cairo_pattern_t *bg;
  GMainLoop *loop;
  cairo_t *cr;

  g_assert (gtk_widget_get_realized (widget));

  surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               gtk_widget_get_allocated_width (widget),
                                               gtk_widget_get_allocated_height (widget));

  loop = g_main_loop_new (NULL, FALSE);
  /* We wait until the widget is drawn for the first time.
   * We can not wait for a GtkWidget::draw event, because that might not
   * happen if the window is fully obscured by windowed child widgets.
   * Alternatively, we could wait for an expose event on widget's window.
   * Both of these are rather hairy, not sure what's best. */
  gdk_event_handler_set (check_for_draw, loop, NULL);
  g_main_loop_run (loop);

  cr = cairo_create (surface);

  switch (mode)
    {
    case SNAPSHOT_WINDOW:
      {
        GdkWindow *window = gtk_widget_get_window (widget);
        if (gdk_window_get_window_type (window) == GDK_WINDOW_TOPLEVEL ||
            gdk_window_get_window_type (window) == GDK_WINDOW_FOREIGN)
          {
            /* give the WM/server some time to sync. They need it.
             * Also, do use popups instead of toplevls in your tests
             * whenever you can. */
            gdk_display_sync (gdk_window_get_display (window));
            g_timeout_add (500, quit_when_idle, loop);
            g_main_loop_run (loop);
          }
        gdk_cairo_set_source_window (cr, window, 0, 0);
        cairo_paint (cr);
      }
      break;
    case SNAPSHOT_DRAW:
      bg = gdk_window_get_background_pattern (gtk_widget_get_window (widget));
      if (bg)
        {
          cairo_set_source (cr, bg);
          cairo_paint (cr);
        }
      gtk_widget_draw (widget, cr);
      break;
    default:
      g_assert_not_reached();
      break;
    }

  cairo_destroy (cr);
  g_main_loop_unref (loop);
  gtk_widget_destroy (widget);

  return surface;
}

int main (int argc, char **argv)
{
  GList *toplevels;
  GList *node;

  /* If there's no DISPLAY, we silently error out.  We don't want to break
   * headless builds. */
  if (! gtk_init_check (&argc, &argv))
    return 0;

  toplevels = get_all_widgets ();

  for (node = toplevels; node; node = g_list_next (node))
    {
      WidgetInfo *info;
      char *filename;
      cairo_surface_t *surface;

      info = node->data;

      gtk_widget_show (info->window);

      surface = snapshot_widget (info->window,
                                 info->include_decorations ? SNAPSHOT_WINDOW : SNAPSHOT_DRAW);
      filename = g_strdup_printf ("./%s.png", info->name);
      g_assert (cairo_surface_write_to_png (surface, filename) == CAIRO_STATUS_SUCCESS);
      g_free (filename);
      cairo_surface_destroy (surface);
    }

  return 0;
}
