/* Canvas/Intro
 *
 * GtkCanvas is a very powerful canvas widget. Here is
 * a simple Hello World demo to get accustomed to how
 * it works.
 */

#include <gtk/gtk.h>

#define WIDTH 400
#define HEIGHT 300

static gboolean
center_item (GtkCanvasItem *ci,
             GtkCanvasBox  *out_box,
             gpointer       unused)
{
  const GtkCanvasBox *viewport;
  GtkWidget *widget;
  int width, height;

  /* We need to check if the viewport is available.
   * If the canvas was scrolling, it might not be avaiable yet.
   */
  viewport = gtk_canvas_get_viewport (gtk_canvas_item_get_canvas (ci));
  if (viewport == NULL)
    return FALSE;

  /* Measure the widget min for min so that it will line-break. */
  widget = gtk_canvas_item_get_widget (ci);
  gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1, &width, NULL, NULL, NULL);
  gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, width, &height, NULL, NULL, NULL);

  /* Initialize the bounds for this widget:
   * - the point is the center of the canvas' viewport
   * - the size is the one we just computed
   * - we want the origin point to be at the center
   */
  gtk_canvas_box_init (out_box,
                       viewport->size.width * 0.5,
                       viewport->size.height * 0.5,
                       0, 0,
                       0.5, 0.5);

  return TRUE;
}

static void
bind_item (GtkListItemFactory *factory,
           GtkCanvasItem      *ci)
{
  gtk_canvas_item_set_widget (ci, gtk_canvas_item_get_item (ci));

  /* Set a function to compute the position */
  gtk_canvas_item_set_compute_bounds (ci,
                                      center_item,
                                      NULL, NULL);
}

GtkWidget *
do_canvas_intro (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *canvas, *widget;
      GListStore *store;
      GtkListItemFactory *factory;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), WIDTH, HEIGHT);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      /* GtkCanvas manages its items using an external list.
       * We do a very simple thing and put the widgets in the list
       * that the canvas should display.
       */
      store = g_list_store_new (GTK_TYPE_WIDGET);
      widget = gtk_label_new ("Hello World");
      gtk_label_set_wrap (GTK_LABEL (widget), TRUE);
      g_list_store_append (store, widget);

      /* GtkCanvas maps the items from the list to the canvas using factories.
       * Set up a simple factory here that just maps the widget directly
       * onto the canvas.
       */
      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);

      /* Create the canvas.
       * We hand it the factory and the model, and then everything happens by itself.
       */
      canvas = gtk_canvas_new (G_LIST_MODEL (store), factory);
      gtk_window_set_child (GTK_WINDOW (window), canvas);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
