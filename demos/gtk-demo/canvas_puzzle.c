/* Canvas/Intro
 *
 * GtkCanvas is a very powerful canvas widget. Here is
 * a simple Hello World demo to get accustomed to how
 * it works.
 */

#include <gtk/gtk.h>

#include "puzzlepiece.h"

static void
set_item_position (GtkCanvasItem *ci,
                   float          x,
                   float          y)
{
  GtkCanvasVector *point, *size;
  GtkCanvasBox *box, *viewport;

  x = CLAMP (x, 0, 1);
  y = CLAMP (y, 0, 1);

  point = gtk_canvas_vector_new (0, 0);
  viewport = gtk_canvas_box_new (point,
                                 gtk_canvas_get_viewport_size (gtk_canvas_item_get_canvas (ci)),
                                 0.0, 0.0);
  gtk_canvas_vector_free (point);

  point = gtk_canvas_vector_new_from_box (viewport, x, y);
  gtk_canvas_box_free (viewport);
  size = gtk_canvas_vector_new (0, 0);
  box = gtk_canvas_box_new (point, size, x, y);
  gtk_canvas_vector_free (point);
  gtk_canvas_vector_free (size);

  gtk_canvas_item_set_bounds (ci, box);
  gtk_canvas_box_free (box);
}

static void
move_item (GtkGestureDrag *gesture,
           double          x,
           double          y,
           GtkCanvasItem  *ci)
{
  GtkCanvas *canvas = gtk_canvas_item_get_canvas (ci);
  graphene_rect_t bounds;

  if (!gtk_canvas_box_eval (gtk_canvas_item_get_bounds (ci), &bounds))
    return;

  set_item_position (ci,
                     (bounds.origin.x + x) / (gtk_widget_get_width (GTK_WIDGET (canvas)) - bounds.size.width),
                     (bounds.origin.y + y) / (gtk_widget_get_height (GTK_WIDGET (canvas)) - bounds.size.height));
}

static void
bind_item (GtkListItemFactory *factory,
           GtkCanvasItem      *ci)
{
  GtkWidget *widget;
  GtkGesture *gesture;

  widget = gtk_picture_new_for_paintable (gtk_canvas_item_get_item (ci));
  gtk_picture_set_can_shrink (GTK_PICTURE (widget), FALSE);
  gesture = gtk_gesture_drag_new ();
  g_signal_connect (gesture, "drag-update", G_CALLBACK (move_item), ci);
  g_signal_connect (gesture, "drag-end", G_CALLBACK (move_item), ci);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));
  gtk_canvas_item_set_widget (ci, widget);

  /* Also center the item, so we do something interesting */
  set_item_position (ci, g_random_double (), g_random_double ());
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
