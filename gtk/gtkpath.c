#include "config.h"
#include "gtkpath.h"
#include "gskcurveprivate.h"

typedef struct
{
  graphene_point_t point;
  float weight;
} PointWeight;

typedef struct
{
  GskPathOperation op;
  gsize idx;
} PathOp;

struct _GtkPath
{
  GObject parent_instance;

  GArray *ops;
  GArray *points;
};

struct _GtkPathClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GtkPath, gtk_path, G_TYPE_OBJECT)

static void
gtk_path_init (GtkPath *self)
{
  self->ops = g_array_new (FALSE, FALSE, sizeof (PathOp));
  self->points = g_array_new (FALSE, FALSE, sizeof (PointWeight));
}

static void
gtk_path_finalize (GObject *object)
{
  GtkPath *self = GTK_PATH (object);

  g_array_unref (self->ops);
  g_array_unref (self->points);

  G_OBJECT_GET_CLASS (object)->finalize (object);
}

static void
gtk_path_class_init (GtkPathClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_path_finalize;
}

GtkPath *
gtk_path_new (void)
{
  return g_object_new (GTK_TYPE_PATH, NULL);
}

static gboolean
add_curve (GskPathOperation        op,
           const graphene_point_t *pts,
           gsize                   n_pts,
           float                   weight,
           gpointer                user_data)
{
  GtkPath *self = GTK_PATH (user_data);

  switch (op)
    {
    case GSK_PATH_MOVE:
      g_array_append_val (self->ops, ((PathOp) { op, self->points->len }));
      g_array_append_val (self->points, ((PointWeight) { pts[0], 1.f } ));
      break;

    case GSK_PATH_CLOSE:
      g_array_append_val (self->ops, ((PathOp) { op, self->points->len - 1 }));
      break;

    case GSK_PATH_LINE:
      g_array_append_val (self->ops, ((PathOp) { op, self->points->len - 1 }));
      g_array_append_val (self->points, ((PointWeight) { pts[1], 1.f } ));
      break;

    case GSK_PATH_QUAD:
      g_array_append_val (self->ops, ((PathOp) { op, self->points->len - 1 }));
      g_array_append_val (self->points, ((PointWeight) { pts[1], 1.f } ));
      g_array_append_val (self->points, ((PointWeight) { pts[2], 1.f } ));
      break;

    case GSK_PATH_CUBIC:
      g_array_append_val (self->ops, ((PathOp) { op, self->points->len - 1 }));
      g_array_append_val (self->points, ((PointWeight) { pts[1], 1.f } ));
      g_array_append_val (self->points, ((PointWeight) { pts[2], 1.f } ));
      g_array_append_val (self->points, ((PointWeight) { pts[3], 1.f } ));
      break;

    case GSK_PATH_CONIC:
      g_array_append_val (self->ops, ((PathOp) { op, self->points->len - 1 }));
      g_array_append_val (self->points, ((PointWeight) { pts[1], weight } ));
      g_array_append_val (self->points, ((PointWeight) { pts[2], 1.f } ));
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
gtk_path_clear (GtkPath *self)
{
  g_array_set_size (self->ops, 0);
  g_array_set_size (self->points, 0);
}

void
gtk_path_set_gsk_path (GtkPath *self,
                       GskPath *path)
{
  g_return_if_fail (GTK_IS_PATH (self));
  g_return_if_fail (path != NULL);

  gtk_path_clear (self);
  gsk_path_foreach (path, -1, add_curve, self);
}

GskPath *
gtk_path_get_gsk_path (GtkPath *self)
{
  GskPathBuilder *builder;

  g_return_val_if_fail (GTK_IS_PATH (self), NULL);

  builder = gsk_path_builder_new ();

  for (gsize i = 0; i < self->ops->len; i++)
    {
      PathOp op = g_array_index (self->ops, PathOp, i);
      PointWeight *p0, *p1, *p2;
      switch (op.op)
        {
        case GSK_PATH_MOVE:
          p0 = &g_array_index (self->points, PointWeight, op.idx);
          gsk_path_builder_move_to (builder, p0->point.x, p0->point.y);
          break;

        case GSK_PATH_CLOSE:
          gsk_path_builder_close (builder);
          break;

        case GSK_PATH_LINE:
          p0 = &g_array_index (self->points, PointWeight, op.idx);
          gsk_path_builder_line_to (builder, p0->point.x, p0->point.y);
          break;

        case GSK_PATH_QUAD:
          p0 = &g_array_index (self->points, PointWeight, op.idx);
          p1 = &g_array_index (self->points, PointWeight, op.idx + 1);
          gsk_path_builder_quad_to (builder,
                                    p0->point.x, p0->point.y,
                                    p1->point.x, p1->point.y);
          break;

        case GSK_PATH_CUBIC:
          p0 = &g_array_index (self->points, PointWeight, op.idx);
          p1 = &g_array_index (self->points, PointWeight, op.idx + 1);
          p2 = &g_array_index (self->points, PointWeight, op.idx + 2);
          gsk_path_builder_cubic_to (builder,
                                     p0->point.x, p0->point.y,
                                     p1->point.x, p1->point.y,
                                     p2->point.x, p2->point.y);
          break;

        case GSK_PATH_CONIC:
          p0 = &g_array_index (self->points, PointWeight, op.idx);
          p1 = &g_array_index (self->points, PointWeight, op.idx + 1);
          gsk_path_builder_conic_to (builder,
                                     p0->point.x, p0->point.y,
                                     p1->point.x, p1->point.y,
                                     p0->weight);
          break;

          default:
            g_assert_not_reached ();
        }
    }

  return gsk_path_builder_free_to_path (builder);
}

gsize
gtk_path_get_n_operations (GtkPath *self)
{
  g_return_val_if_fail (GTK_IS_PATH (self), 0);

  return self->ops->len;
}

GskPathOperation
gtk_path_get_operation (GtkPath *self,
                        gsize    idx)
{
  g_return_val_if_fail (GTK_IS_PATH (self), GSK_PATH_MOVE);
  g_return_val_if_fail (idx < self->ops->len, GSK_PATH_MOVE);

  return g_array_index (self->ops, GskPathOperation, idx);
}

#define OP_CHANGE(op1,op2) ((op1) | ((op2) << 16))

static void
shift_indices (GtkPath *self,
               gsize    from,
               int      shift)
{
  for (gsize i = from; i < self->ops->len; i++)
    {
      PathOp *op = &g_array_index (self->ops, PathOp, i);
      op->idx += shift;
    }
}

static int
line_intersection (const graphene_point_t *a,
                   const graphene_point_t *b,
                   const graphene_point_t *c,
                   const graphene_point_t *d,
                   graphene_point_t       *p)
{ 
  float a1 = b->y - a->y;
  float b1 = a->x - b->x;
  float c1 = a1*a->x + b1*a->y;
  
  float a2 = d->y - c->y;
  float b2 = c->x - d->x;
  float c2 = a2*c->x+ b2*c->y;

  float det = a1*b2 - a2*b1;
  
  if (fabs (det) < 0.001)
    {
      p->x = NAN;
      p->y = NAN;
      return 0;
    }
  else
    {
      p->x = (b2*c1 - b1*c2) / det;
      p->y = (a1*c2 - a2*c1) / det;
      return 1;
    }
}

static void
get_curve (GtkPath *self, gsize idx, GskCurve *curve)
{
  PathOp op = g_array_index (self->ops, PathOp, idx);
  PointWeight *pw = &g_array_index (self->points, PointWeight, op.idx);

  switch (op.op)
    {
    case GSK_PATH_LINE:
      gsk_curve_init_foreach (curve,
                              op.op,
                              (graphene_point_t[]) { pw[0].point, pw[1].point },
                              2,
                              1.f);
      break;
    case GSK_PATH_QUAD:
      gsk_curve_init_foreach (curve,
                              op.op,
                              (graphene_point_t[]) { pw[0].point, pw[1].point, pw[2].point },
                              3,
                              1.f);
      break;
    case GSK_PATH_CUBIC:
      gsk_curve_init_foreach (curve,
                              op.op,
                              (graphene_point_t[]) { pw[0].point, pw[1].point, pw[2].point, pw[3].point },
                              4,
                              1.f);
      break;
    case GSK_PATH_CONIC:
      gsk_curve_init_foreach (curve,
                              op.op,
                              (graphene_point_t[]) { pw[0].point, pw[1].point, pw[2].point },
                              3,
                              pw[1].weight);
      break;
    case GSK_PATH_MOVE:
    case GSK_PATH_CLOSE:
    default:
      g_assert_not_reached ();
    }
}

void
gtk_path_set_operation (GtkPath          *self,
                        gsize             idx,
                        GskPathOperation  op)
{
  PathOp *old;
  const graphene_point_t *p0, *p1, *p2, *p3;

  g_return_if_fail (GTK_IS_PATH (self));
  g_return_if_fail (idx < self->ops->len);

  old = &g_array_index (self->ops, PathOp, idx);

  if (old->op == op)
    return;

  /* Can't change the move, for now */
  if (old->op == GSK_PATH_MOVE)
    return;

  switch (old->op)
    {
    case GSK_PATH_CLOSE:
      p0 = p1 = &(g_array_index (self->points, PointWeight, old->idx).point);
      for (gsize idx2 = idx; idx2 > 0; idx2--)
        {
          PathOp op2 = g_array_index (self->ops, PathOp, idx2 - 1);
          if (op2.op == GSK_PATH_MOVE)
            {
              p1 = &(g_array_index (self->points, PointWeight, op2.idx).point);
              break;
            }
        }
      switch (op)
        {
        case GSK_PATH_LINE:
          {
            graphene_point_t q;

            /* Not putting the point quite on top */
            graphene_point_interpolate (p0, p1, 0.9, &q);
            g_array_insert_val (self->points, old->idx + 1, ((PointWeight) { q, 1.f }));
            shift_indices (self, idx + 1, 1);
            old->op = op;
          }
          break;

        case GSK_PATH_QUAD:
        case GSK_PATH_CONIC:
          {
            graphene_point_t q1, q2;

            /* Not putting the point quite on top */
            graphene_point_interpolate (p0, p1, 0.9, &q2);
            graphene_point_interpolate (p0, &q1, 0.5, &q1);
            g_array_insert_vals (self->points, old->idx + 1,
                                 (PointWeight[]) { { q1, 1.f }, { q2, 1.f } },
                                 2);
            shift_indices (self, idx + 1, 2);
            old->op = op;
          }
          break;

        case GSK_PATH_CUBIC:
          {
            graphene_point_t q1, q2, q3;

            /* Not putting the point quite on top */
            graphene_point_interpolate (p0, p1, 0.9, &q3);
            graphene_point_interpolate (p0, &q3, 0.333, &q1);
            graphene_point_interpolate (p0, &q3, 0.667, &q2);
            g_array_insert_vals (self->points, old->idx + 1,
                                 (PointWeight[]) { { q1, 1.f }, { q2, 1.f }, { q3, 1.f }  },
                                 3);
            shift_indices (self, idx + 1, 3);
            old->op = op;
          }
          break;

        case GSK_PATH_MOVE:
        case GSK_PATH_CLOSE:
        default:
          g_assert_not_reached ();
        }
      break;

    case GSK_PATH_LINE:
      p0 = &(g_array_index (self->points, PointWeight, old->idx).point);
      p1 = &(g_array_index (self->points, PointWeight, old->idx + 1).point);

      switch (op)
        {
        case GSK_PATH_CLOSE:
          break;

        case GSK_PATH_QUAD:
        case GSK_PATH_CONIC:
          {
            graphene_point_t q;

            graphene_point_interpolate (p0, p1, 0.5, &q);
            g_array_insert_val (self->points, old->idx + 1, ((PointWeight) { q, 1.f }));
            shift_indices (self, idx + 1, 1);
            old->op = op;
          }
          break;

        case GSK_PATH_CUBIC:
          {
            graphene_point_t q1, q2;

            graphene_point_interpolate (p0, p1, 0.333, &q1);
            graphene_point_interpolate (p0, p1, 0.667, &q2);

            g_array_insert_vals (self->points, old->idx + 1,
                                 (PointWeight[]) { { q1, 1.f }, { q2, 1.f } },
                                 2);
            shift_indices (self, idx + 1, 2);
            old->op = op;
          }
          break;

        case GSK_PATH_LINE:
        case GSK_PATH_MOVE:
        default:
          g_assert_not_reached ();
        }
      break;

    case GSK_PATH_QUAD:
    case GSK_PATH_CONIC:
      p0 = &(g_array_index (self->points, PointWeight, old->idx)).point;
      p1 = &(g_array_index (self->points, PointWeight, old->idx + 1)).point;
      p2 = &(g_array_index (self->points, PointWeight, old->idx + 2)).point;

      switch (op)
        {
        case GSK_PATH_QUAD:
        case GSK_PATH_CONIC:
          old->op = op;
          break;

        case GSK_PATH_CUBIC:
          {
            graphene_point_t q1, q2;

            graphene_point_interpolate (p0, p1, 0.667, &q1);
            graphene_point_interpolate (p1, p2, 0.333, &q2);

            g_array_insert_vals (self->points, old->idx + 1,
                                 (PointWeight[]) { { q1, 1.f }, { q2, 1.f } },
                                 2);
            shift_indices (self, idx + 1, 2);
            old->op = op;
          }
          break;

        case GSK_PATH_LINE:
          g_array_remove_index (self->points, old->idx + 1);
          shift_indices (self, idx + 1, -1);
          old->op = op;
          break;

        case GSK_PATH_MOVE:
        case GSK_PATH_CLOSE:
        default:
          g_assert_not_reached ();
        }
      break;

    case GSK_PATH_CUBIC:
      p0 = &(g_array_index (self->points, PointWeight, old->idx)).point;
      p1 = &(g_array_index (self->points, PointWeight, old->idx + 1)).point;
      p2 = &(g_array_index (self->points, PointWeight, old->idx + 2)).point;
      p3 = &(g_array_index (self->points, PointWeight, old->idx + 3)).point;

      switch (op)
        {
        case GSK_PATH_CLOSE:
          break;

        case GSK_PATH_LINE:
          g_array_remove_range (self->points, old->idx + 1, 2);
          shift_indices (self, idx + 1, -2);
          old->op = op;
          break;

        case GSK_PATH_QUAD:
        case GSK_PATH_CONIC:
          {
            graphene_point_t q;
            PointWeight *pw;

            if (!line_intersection (p0, p1, p2, p3, &q))
              graphene_point_interpolate (p0, p3, 0.5, &q);
            g_array_remove_index (self->points, old->idx + 2);
            pw = &g_array_index (self->points, PointWeight, old->idx + 1);
            pw->point = q;
            shift_indices (self, idx + 1, -1);
            old->op = op;
          }
          break;

        case GSK_PATH_MOVE:
        case GSK_PATH_CUBIC:
        default:
          g_assert_not_reached ();
        }
      break;

    case GSK_PATH_MOVE:
    default:
      g_assert_not_reached ();
    }
}

void
gtk_path_split_operation (GtkPath *self,
                          gsize    idx,
                          float    t)
{
  PathOp op;
  GskCurve curve, c1, c2;

  g_return_if_fail (GTK_IS_PATH (self));
  g_return_if_fail (idx < self->ops->len);
  g_return_if_fail (0 <= t && t <= 1);

  op = g_array_index (self->ops, PathOp, idx);

  if (op.op == GSK_PATH_MOVE || op.op == GSK_PATH_CLOSE)
    return; /* Can't do these for now */

  get_curve (self, idx, &curve);
  gsk_curve_split (&curve, t, &c1, &c2);

  switch (op.op)
    {
    case GSK_PATH_LINE:
      g_array_insert_val (self->points, op.idx + 1,
                          ((PointWeight) { c1.line.points[1], 1.f }));
      shift_indices (self, idx + 1, 1);
      g_array_insert_val (self->ops, idx + 1, ((PathOp) { op.op, op.idx + 1 }));
      break;

    case GSK_PATH_QUAD:
      g_array_remove_index (self->points, op.idx + 1);
      g_array_insert_vals (self->points, op.idx + 1,
                           (PointWeight []) { { c1.quad.points[1], 1.f },
                                              { c1.quad.points[2], 1.f },
                                              { c2.quad.points[1], 1.f } },
                           3);
      shift_indices (self, idx + 1, 2);
      g_array_insert_val (self->ops, idx + 1, ((PathOp) { op.op, op.idx + 2 }));
      break;

    case GSK_PATH_CUBIC:
      g_array_remove_range (self->points, op.idx + 1, 2);
      g_array_insert_vals (self->points, op.idx + 1,
                           (PointWeight []) { { c1.cubic.points[1], 1.f },
                                              { c1.cubic.points[2], 1.f },
                                              { c1.cubic.points[3], 1.f },
                                              { c2.cubic.points[1], 1.f },
                                              { c2.cubic.points[2], 1.f } },
                           5);
      shift_indices (self, idx + 1, 3);
      g_array_insert_val (self->ops, idx + 1, ((PathOp) { op.op, op.idx + 3 }));
      break;

    case GSK_PATH_CONIC:
      g_array_remove_index (self->points, op.idx + 1);
      g_array_insert_vals (self->points, op.idx + 1,
                           (PointWeight []) { { c1.conic.points[1], c1.conic.points[2].x },
                                              { c1.conic.points[3], 1.f },
                                              { c2.conic.points[1], c2.conic.points[2].x } },
                           3);
      shift_indices (self, idx + 1, 2);
      g_array_insert_val (self->ops, idx + 1, ((PathOp) { op.op, op.idx + 2 }));
      break;

    case GSK_PATH_MOVE:
    case GSK_PATH_CLOSE:
    default:
      g_assert_not_reached ();
    }
}

void
gtk_path_remove_operation (GtkPath *self,
                           gsize    idx)
{
  PathOp op;

  g_return_if_fail (GTK_IS_PATH (self));
  g_return_if_fail (idx < self->ops->len);

  op = g_array_index (self->ops, PathOp, idx);

  switch (op.op)
    {
    case GSK_PATH_MOVE:
      /* Removing a MOVE is a no-op */
      return;

    case GSK_PATH_CLOSE:
      /* No point point shuffling needed */
      g_array_remove_index (self->ops, idx);
      return;

    case GSK_PATH_LINE:
      g_array_remove_index (self->ops, idx);
      g_array_remove_range (self->points, op.idx, 1);
      shift_indices (self, idx, -1);
      break;

    case GSK_PATH_QUAD:
    case GSK_PATH_CONIC:
      g_array_remove_index (self->ops, idx);
      g_array_remove_range (self->points, op.idx, 2);
      shift_indices (self, idx, -2);
      break;

    case GSK_PATH_CUBIC:
      g_array_remove_index (self->ops, idx);
      g_array_remove_range (self->points, op.idx, 3);
      shift_indices (self, idx, -3);
      break;

    default:
      g_assert_not_reached ();
    }
}

gsize
gtk_path_get_points_for_operation (GtkPath *self,
                                   gsize    idx)
{
  PathOp op;

  g_return_val_if_fail (GTK_IS_PATH (self), 0);
  g_return_val_if_fail (idx < self->ops->len, 0);

  op = g_array_index (self->ops, PathOp, idx);

  return op.idx;
}

gsize
gtk_path_get_n_points (GtkPath *self)
{
  g_return_val_if_fail (GTK_IS_PATH (self), 0);

  return self->points->len;
}

void
gtk_path_get_point (GtkPath          *self,
                    gsize             idx,
                    graphene_point_t *point)
{
  PointWeight *p;

  g_return_if_fail (GTK_IS_PATH (self));
  g_return_if_fail (idx < self->points->len);
  g_return_if_fail (point != NULL);

  p = &g_array_index (self->points, PointWeight, idx);
  *point = p->point;
}

void
gtk_path_set_point (GtkPath                *self,
                    gsize                   idx,
                    const graphene_point_t *point)
{
  PointWeight *p;

  g_return_if_fail (GTK_IS_PATH (self));
  g_return_if_fail (idx < self->points->len);
  g_return_if_fail (point != NULL);

  p = &g_array_index (self->points, PointWeight, idx);
  p->point = *point;
}

float
gtk_path_get_conic_weight (GtkPath *self,
                           gsize    idx)
{
  PointWeight *p;

  g_return_val_if_fail (GTK_IS_PATH (self), 1.f);
  g_return_val_if_fail (idx < self->points->len, 1.f);

  p = &g_array_index (self->points, PointWeight, idx);
  return p->weight;
}

void
gtk_path_set_conic_weight (GtkPath *self,
                           gsize    idx,
                           float    weight)
{
  PointWeight *p;

  g_return_if_fail (GTK_IS_PATH (self));
  g_return_if_fail (idx < self->points->len);
  g_return_if_fail (weight > 0);

  p = &g_array_index (self->points, PointWeight, idx);
  p->weight = weight;
}
