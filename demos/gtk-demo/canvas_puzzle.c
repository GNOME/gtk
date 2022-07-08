/* Canvas/Intro
 *
 * GtkCanvas is a very powerful canvas widget. Here is
 * a simple Hello World demo to get accustomed to how
 * it works.
 */

#include <gtk/gtk.h>

#include "puzzlepiece.h"

static gboolean
set_position_from_origin (GtkCanvasItem *ci,
                          GtkCanvasBox  *out_box,
                          gpointer       user_data)
{
  GtkOrigin *origin = g_object_get_data (G_OBJECT (ci), "position");
  const GtkCanvasBox *viewport = gtk_canvas_get_viewport (gtk_canvas_item_get_canvas (ci));

  if (viewport == NULL)
    return FALSE;

  gtk_canvas_box_init (out_box,
                       viewport->size.width * origin->horizontal,
                       viewport->size.height * origin->vertical,
                       0, 0,
                       origin->horizontal,
                       origin->vertical);

  return TRUE;
}

static void
move_item (GtkGestureDrag *gesture,
           double          x,
           double          y,
           GtkCanvasItem  *ci)
{
  GtkCanvas *canvas = gtk_canvas_item_get_canvas (ci);
  GtkOrigin *origin = g_object_get_data (G_OBJECT (ci), "position");

  origin->horizontal += x / gtk_widget_get_width (GTK_WIDGET (canvas));
  origin->vertical += y / gtk_widget_get_height (GTK_WIDGET (canvas));
  origin->horizontal = CLAMP (origin->horizontal, 0, 1);
  origin->vertical = CLAMP (origin->vertical, 0, 1);

  gtk_canvas_item_invalidate_bounds (ci);
}

static void
bind_item (GtkListItemFactory *factory,
           GtkCanvasItem      *ci)
{
  GtkWidget *widget;
  GtkGesture *gesture;
  GtkOrigin *origin;

  widget = gtk_picture_new_for_paintable (gtk_canvas_item_get_item (ci));
  gtk_picture_set_can_shrink (GTK_PICTURE (widget), FALSE);
  gesture = gtk_gesture_drag_new ();
  g_signal_connect (gesture, "drag-update", G_CALLBACK (move_item), ci);
  g_signal_connect (gesture, "drag-end", G_CALLBACK (move_item), ci);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));
  gtk_canvas_item_set_widget (ci, widget);

  /* Set a random position */
  origin = g_new (GtkOrigin, 1);
  origin->horizontal = g_random_double ();
  origin->vertical = g_random_double ();
  g_object_set_data_full (G_OBJECT (ci), "position", origin, g_free);

  g_signal_connect (ci, "compute-bounds", G_CALLBACK (set_position_from_origin), NULL);
}

static GListModel *
create_puzzle (GdkPaintable *puzzle)
{
  GListStore *store = g_list_store_new (GDK_TYPE_PAINTABLE);
  int width = 5;
  int height = 5;
  int x, y;

  /* add a picture for every cell */
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          GdkPaintable *piece;

          piece = gtk_puzzle_piece_new (puzzle,
                                        x, y,
                                        width, height);
          g_list_store_append (store, piece);
          g_object_unref (piece);
        }
    }

  return G_LIST_MODEL (store);
}

GtkWidget *
do_canvas_puzzle (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *canvas;
      GListModel *model;
      GtkListItemFactory *factory;
      GdkPaintable *puzzle;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      puzzle = GDK_PAINTABLE (gdk_texture_new_from_resource ("/sliding_puzzle/portland-rose.jpg"));
      model = create_puzzle (puzzle);
      g_object_unref (puzzle);

      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);

      canvas = gtk_canvas_new (model, factory);
      gtk_window_set_child (GTK_WINDOW (window), canvas);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
