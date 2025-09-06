#include <math.h>
#include "minigraph.h"

typedef enum {
  IDENTITY,
  LEVELS,
  LINEAR,
  GAMMA,
  DISCRETE,
  TABLE,
} FilterKind;

struct _MiniGraph
{
  GtkWidget parent_instance;

  FilterKind filter_kind;

  union {
    struct {
      guint n;
    } levels;

    struct {
      float m;
      float b;
    } linear;

    struct {
      float amp;
      float exp;
      float ofs;
    } gamma;

    struct {
      guint n;
      float values[6];
    } discrete;

    struct {
      guint n;
      float values[6];
    } table;
  };
};

struct _MiniGraphClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (MiniGraph, mini_graph, GTK_TYPE_WIDGET)

static void
mini_graph_init (MiniGraph *self)
{
}

static void
mini_graph_dispose (GObject *object)
{
  //MiniGraph *self = MINI_GRAPH (object);

  G_OBJECT_CLASS (mini_graph_parent_class)->dispose (object);
}

static GskPath *
create_path (MiniGraph *self,
             int        size)
{
  GskPathBuilder *builder = gsk_path_builder_new ();

  switch (self->filter_kind)
    {
    case IDENTITY:
      gsk_path_builder_move_to (builder, 0, size);
      gsk_path_builder_line_to (builder, size, 0);
      break;

    case LEVELS:
      {
        float x0, x1, x, y;

        x1 = 0;
        for (guint i = 0; i < self->levels.n; i++)
          {
            x0 = x1;
            x1 = (i + 1) / (float) self->levels.n;
            x = (x0 + x1) / 2;
            y = (floorf (x * self->levels.n) + 0.5) / self->levels.n;

            gsk_path_builder_move_to (builder, x0 * size, (1 - y) * size);
            gsk_path_builder_line_to (builder, x1 * size, (1 - y) * size);
          }
      }
      break;

    case LINEAR:
      {
        float y0, y1, x0, x1;

        y0 = self->linear.b;
        y1 = self->linear.m + self->linear.b;

        if (y0 <= 0)
          {
            if (y1 <= 0)
              {
                gsk_path_builder_move_to (builder, 0, size);
                gsk_path_builder_line_to (builder, size, size);
              }
            else if (y1 <= 1)
              {
                x0 = - self->linear.b / self->linear.m;

                gsk_path_builder_move_to (builder, 0, size);
                gsk_path_builder_line_to (builder, x0 * size, size);
                gsk_path_builder_line_to (builder, size, (1 - y1) * size);
              }
            else
              {
                x0 = - self->linear.b / self->linear.m;
                x1 = (1 - self->linear.b) / self->linear.m;

                gsk_path_builder_move_to (builder, 0, size);
                gsk_path_builder_line_to (builder, x0 * size, size);
                gsk_path_builder_line_to (builder, x1 * size, 0);
                gsk_path_builder_line_to (builder, size, 0);
              }
          }
        else if (y0 <= 1)
          {
            if (y1 <= 0)
              {
                x0 = - self->linear.b / self->linear.m;

                gsk_path_builder_move_to (builder, 0, (1 - y0) * size);
                gsk_path_builder_line_to (builder, x0 * size, size);
                gsk_path_builder_line_to (builder, size, size);
              }
            else if (y1 <= 1)
              {
                gsk_path_builder_move_to (builder, 0, (1 - y0) * size);
                gsk_path_builder_line_to (builder, size, (1 - y1) * size);
              }
            else
              {
                x1 = (1 - self->linear.b) / self->linear.m;

                gsk_path_builder_move_to (builder, 0, (1 - y0) * size);
                gsk_path_builder_line_to (builder, x1 * size, 0);
                gsk_path_builder_line_to (builder, size, 0);
              }
          }
        else
          {
            if (y1 <= 0)
              {
                x0 = (1 - self->linear.b) / self->linear.m;
                x1 = - self->linear.b / self->linear.m;

                gsk_path_builder_move_to (builder, 0, 0);
                gsk_path_builder_line_to (builder, x0 * size, 0);
                gsk_path_builder_line_to (builder, x1 * size, size);
                gsk_path_builder_line_to (builder, size, size);
              }
            else if (y1 <= 1)
              {
                x0 = (1 - self->linear.b) / self->linear.m;

                gsk_path_builder_move_to (builder, 0, 0);
                gsk_path_builder_line_to (builder, x0 * size, 0);
                gsk_path_builder_line_to (builder, size, (1 - y1) * size);
              }
            else
              {
                gsk_path_builder_move_to (builder, 0, 0);
                gsk_path_builder_line_to (builder, size, 0);
              }
          }
      }
      break;

    case GAMMA:
      {
        float x, y;

        for (int i = 0; i <= size; i++)
          {
            x = i / (float) size;
            y = self->gamma.amp * powf (x, self->gamma.exp) + self->gamma.ofs;
            y = CLAMP (y, 0, 1);

            if (i == 0)
              gsk_path_builder_move_to (builder, x * size, (1 - y) * size);
            else
              gsk_path_builder_line_to (builder, x * size, (1 - y) * size);
          }
      }
      break;

    case DISCRETE:
      {
        float x0, x1, y;

        x1 = 0;
        for (guint i = 0; i < self->discrete.n; i++)
          {
            x0 = x1;
            x1 = (i + 1) / (float) self->discrete.n;
            y = self->discrete.values[i];

            gsk_path_builder_move_to (builder, x0 * size, (1 - y) * size);
            gsk_path_builder_line_to (builder, x1 * size, (1 - y) * size);
          }
      }
      break;

    case TABLE:
      {
        float x, y;

        for (guint i = 0; i < self->table.n; i++)
          {
            x = i / (float) (self->table.n - 1);
            y = self->table.values[i];

            if (i == 0)
              gsk_path_builder_move_to (builder, x * size, (1 - y) * size);
            else
              gsk_path_builder_line_to (builder, x * size, (1 - y) * size);
          }
      }
      break;

    default:
      g_assert_not_reached ();
    }

  return gsk_path_builder_free_to_path (builder);
}

static void
mini_graph_snapshot (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  MiniGraph *self = MINI_GRAPH (widget);
  int width, height, size;
  GskPath *path;
  GskStroke *stroke;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  size = MIN (width, height);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT ((width - size) / 2,
                                                (height - size) / 2));

  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA) { 1, 1, 1, 1 },
                             &GRAPHENE_RECT_INIT (0, 0, size, size));
  gtk_snapshot_append_border (snapshot,
                              &GSK_ROUNDED_RECT_INIT (0, 0, size, size),
                              (float [4]) { 1, 1, 1, 1 },
                              (GdkRGBA[4]) {
                                { 0, 0, 0, 0.75 },
                                { 0, 0, 0, 0.75 },
                                { 0, 0, 0, 0.75 },
                                { 0, 0, 0, 0.75 },
                              });

  path = create_path (self, size);

  stroke = gsk_stroke_new (2);

  gtk_snapshot_append_stroke (snapshot, path, stroke, &(GdkRGBA) { 0, 0, 1, 1 });

  gsk_stroke_free (stroke);

  gsk_path_unref (path);

  gtk_snapshot_restore (snapshot);
}

static void
mini_graph_class_init (MiniGraphClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = mini_graph_dispose;
  widget_class->snapshot = mini_graph_snapshot;
}

GtkWidget *
mini_graph_new (void)
{
  return g_object_new (mini_graph_get_type (), NULL);
}

void
mini_graph_set_identity (MiniGraph *self)
{
  self->filter_kind = IDENTITY;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
mini_graph_set_levels (MiniGraph *self,
                       guint      levels)
{
  self->filter_kind = LEVELS;
  self->levels.n = levels;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
mini_graph_set_linear (MiniGraph *self,
                       float      m,
                       float      b)
{
  self->filter_kind = LINEAR;
  self->linear.m = m;
  self->linear.b = b;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
mini_graph_set_gamma (MiniGraph *self,
                      float      amp,
                      float      exp,
                      float      ofs)
{
  self->filter_kind = GAMMA;
  self->gamma.amp = amp;
  self->gamma.exp = exp;
  self->gamma.ofs = ofs;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
mini_graph_set_discrete (MiniGraph *self,
                         guint      n,
                         float     *values)
{
  self->filter_kind = DISCRETE;
  self->discrete.n = MIN (n, 6);
  for (guint i = 0; i < self->discrete.n; i++)
    self->discrete.values[i] = values[i];

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
mini_graph_set_table (MiniGraph *self,
                      guint      n,
                      float     *values)
{
  self->filter_kind = TABLE;
  self->table.n = MIN (n, 6);
  for (guint i = 0; i < self->table.n; i++)
    self->table.values[i] = values[i];

  gtk_widget_queue_draw (GTK_WIDGET (self));
}
