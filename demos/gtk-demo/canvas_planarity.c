/* Canvas/Planarity
 *
 * This demonstrates how the canvas can be used to display different
 * types of data (vertices and edges) and how to operate on them.
 */

#include <gtk/gtk.h>

#define GTK_TYPE_DIAGONAL_LINE (gtk_diagonal_line_get_type ())
G_DECLARE_FINAL_TYPE (GtkDiagonalLine, gtk_diagonal_line, GTK, DIAGONAL_LINE, GtkWidget)

struct _GtkDiagonalLine
{
  GtkWidget parent_instance;
};

struct _GtkDiagonalLineClass
{
  GtkWidgetClass parent_class;
};

static void
gtk_diagonal_line_snapshot (GtkWidget   *widget,
                            GtkSnapshot *snapshot)
{
  const float line_width = 6;
  const int width = gtk_widget_get_width (widget);
  const int height = gtk_widget_get_height (widget);
  const double length = sqrt (width * width + height * height);
  GskRoundedRect clip;
  GdkRGBA color;

  gtk_snapshot_save (snapshot);
  gtk_snapshot_rotate (snapshot, atan2 (height, width) * 180 / G_PI);

  gsk_rounded_rect_init_from_rect (&clip,
                                   &GRAPHENE_RECT_INIT (
                                     -0.5f * line_width,
                                     -0.5f * line_width,
                                     length + line_width,
                                     line_width),
                                   0.5f * line_width);
  gtk_snapshot_push_rounded_clip (snapshot, &clip);
  gtk_style_context_get_color (gtk_widget_get_style_context (widget), &color);
  gtk_snapshot_append_color (snapshot,
                             &color,
                             &clip.bounds);
  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);
}

/* When defining the GType, we need to implement the GdkPaintable interface */
G_DEFINE_TYPE (GtkDiagonalLine, gtk_diagonal_line, GTK_TYPE_WIDGET)

/* Here's the boilerplate for the GObject declaration.
 */
static void
gtk_diagonal_line_class_init (GtkDiagonalLineClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->snapshot = gtk_diagonal_line_snapshot;
}

static void
gtk_diagonal_line_init (GtkDiagonalLine *self)
{
}

static GtkWidget *
gtk_diagonal_line_new (void)
{
  return g_object_new (GTK_TYPE_DIAGONAL_LINE, NULL);
}

typedef struct _PlanarityVertex PlanarityVertex;
struct _PlanarityVertex
{
  GObject parent_instance;

  GtkOrigin position;
};

#define PLANARITY_TYPE_VERTEX (planarity_vertex_get_type ())
G_DECLARE_FINAL_TYPE (PlanarityVertex, planarity_vertex, PLANARITY, VERTEX, GObject);

G_DEFINE_TYPE (PlanarityVertex, planarity_vertex, G_TYPE_OBJECT);

static void
planarity_vertex_class_init (PlanarityVertexClass *klass)
{
}

static void
planarity_vertex_init (PlanarityVertex *self)
{
}

static PlanarityVertex *
planarity_vertex_new (float horizontal,
                      float vertical)
{
  PlanarityVertex *self;

  self = g_object_new (PLANARITY_TYPE_VERTEX, NULL);

  self->position.horizontal = horizontal;
  self->position.vertical = vertical;

  return self;
}

typedef struct _PlanarityEdge PlanarityEdge;
struct _PlanarityEdge
{
  GObject parent_instance;

  PlanarityVertex *from;
  PlanarityVertex *to;
};

#define PLANARITY_TYPE_EDGE (planarity_edge_get_type ())
G_DECLARE_FINAL_TYPE (PlanarityEdge, planarity_edge, PLANARITY, EDGE, GObject);

G_DEFINE_TYPE (PlanarityEdge, planarity_edge, G_TYPE_OBJECT);

static void
planarity_edge_class_init (PlanarityEdgeClass *klass)
{
}

static void
planarity_edge_init (PlanarityEdge *self)
{
}

static PlanarityEdge *
planarity_edge_new (PlanarityVertex *from,
                    PlanarityVertex *to)
{
  PlanarityEdge *edge;

  edge = g_object_new (PLANARITY_TYPE_EDGE, NULL);

  edge->from = g_object_ref (from);
  edge->to = g_object_ref (to);

  return edge;
}

static gboolean
set_vertex_bounds (GtkCanvasItem *ci,
                   GtkCanvasBox  *out_box,
                   gpointer       user_data)
{
  PlanarityVertex *vertex = gtk_canvas_item_get_item (ci);
  const GtkCanvasBox *viewport;

  viewport = gtk_canvas_get_viewport (gtk_canvas_item_get_canvas (ci));
  if (viewport == NULL)
    return FALSE;

  gtk_canvas_box_init (out_box,
                       viewport->size.width * vertex->position.horizontal,
                       viewport->size.height * vertex->position.vertical,
                       0, 0,
                       vertex->position.horizontal,
                       vertex->position.vertical);

  return TRUE;
}

static void
move_vertex (GtkGestureDrag *gesture,
             double          x,
             double          y,
             GtkCanvasItem  *ci)
{
  GtkCanvas *canvas = gtk_canvas_item_get_canvas (ci);
  PlanarityVertex *vertex = gtk_canvas_item_get_item (ci);

  x /= gtk_widget_get_width (GTK_WIDGET (canvas));
  y /= gtk_widget_get_height (GTK_WIDGET (canvas));

  vertex->position.horizontal = CLAMP (vertex->position.horizontal + x, 0, 1);
  vertex->position.vertical = CLAMP (vertex->position.vertical + y, 0, 1);

  gtk_canvas_item_invalidate_bounds (ci);
}

static gboolean
set_edge_bounds (GtkCanvasItem *ci,
                 GtkCanvasBox  *out_box,
                 gpointer       user_data)
{
  PlanarityEdge *edge = gtk_canvas_item_get_item (ci);
  GtkCanvas *canvas = gtk_canvas_item_get_canvas (ci);
  GtkCanvasItem *from_item, *to_item;
  const GtkCanvasBox *from_box, *to_box;
  graphene_rect_t from_rect, to_rect;
  graphene_point_t from_center, to_center;

  from_item = gtk_canvas_lookup_item (canvas, edge->from);
  to_item = gtk_canvas_lookup_item (canvas, edge->to);
  from_box = gtk_canvas_item_get_allocation (from_item);
  to_box = gtk_canvas_item_get_allocation (to_item);
  if (from_box == NULL || to_box == NULL)
    return FALSE;

  gtk_canvas_box_to_rect (from_box, &from_rect);
  gtk_canvas_box_to_rect (to_box, &to_rect);
  graphene_rect_get_center (&from_rect, &from_center);
  graphene_rect_get_center (&to_rect, &to_center);

  gtk_canvas_box_init (out_box,
                       from_center.x, from_center.y,
                       to_center.x - from_center.x,
                       to_center.y - from_center.y,
                       0, 0);

  return TRUE;
}

static void
bind_item (GtkListItemFactory *factory,
           GtkCanvasItem      *ci)
{
  gpointer item;

  item = gtk_canvas_item_get_item (ci);

  if (PLANARITY_IS_VERTEX (item))
    {
      GtkWidget *widget;
      GtkGesture *gesture;

      widget = gtk_image_new_from_icon_name ("media-record-symbolic");
      gtk_image_set_icon_size (GTK_IMAGE (widget), GTK_ICON_SIZE_LARGE);
      gesture = gtk_gesture_drag_new ();
      g_signal_connect (gesture, "drag-update", G_CALLBACK (move_vertex), ci);
      g_signal_connect (gesture, "drag-end", G_CALLBACK (move_vertex), ci);
      gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));
      gtk_canvas_item_set_widget (ci, widget);

      gtk_canvas_item_set_compute_bounds (ci, set_vertex_bounds, NULL, NULL);
    }
  else if (PLANARITY_IS_EDGE (item))
    {
      gtk_canvas_item_set_widget (ci, gtk_diagonal_line_new ());
      gtk_canvas_item_set_compute_bounds (ci, set_edge_bounds, NULL, NULL);
    }
}

static GListModel *
create_model (void)
{
  GListStore *result, *vertices, *edges;
  guint n = 10;
  guint i, j;

  vertices = g_list_store_new (PLANARITY_TYPE_VERTEX);
  edges = g_list_store_new (PLANARITY_TYPE_EDGE);

  for (i = 0; i < n; i++)
    {
      PlanarityVertex *vertex = planarity_vertex_new (g_random_double (), g_random_double ());
      g_list_store_append (vertices, vertex);

      for (j = 0; j < i; j++)
        {
          PlanarityEdge *edge;
          PlanarityVertex *other;
          if (g_random_boolean ())
            continue;

          other = g_list_model_get_item (G_LIST_MODEL (vertices), j);
          edge = planarity_edge_new (vertex, other);
          g_object_unref (other);
          g_list_store_append (edges, edge);
          g_object_unref (edge);
        }
      g_object_unref (vertex);
    }

  result = g_list_store_new (G_TYPE_LIST_MODEL);
  /* put edges before vertices due to staking order */
  g_list_store_append (result, edges);
  g_object_unref (edges);
  g_list_store_append (result, vertices);
  g_object_unref (vertices);

  return G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (result)));
}

GtkWidget *
do_canvas_planarity (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *canvas;
      GListModel *model;
      GtkListItemFactory *factory;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      model = create_model ();

      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);

      /* Create the canvas.
       * We hand it the factory and the model, and then everything happens by itself.
       */
      canvas = gtk_canvas_new (model, factory);
      gtk_window_set_child (GTK_WINDOW (window), canvas);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
