#include "gtkcontour.h"

#include <gtk/gtk.h>
#include "gsk/gskcurveprivate.h"

typedef struct _GtkCurve GtkCurve;
struct _GtkCurve
{
  GskPathOperation op;
  graphene_point_t p[4];
  float weight;
};

struct _GtkContour
{
  GObject parent_instance;

  GPtrArray *curves;
  graphene_point_t start;
  gboolean closed;
};

struct _GtkContourClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GtkContour, gtk_contour, G_TYPE_OBJECT)


static void
gtk_contour_init (GtkContour *self)
{
  self->curves = g_ptr_array_new_with_free_func (g_free);
  self->closed = FALSE;
}

static void
gtk_contour_finalize (GObject *object)
{
  GtkContour *self = GTK_CONTOUR (object);

  g_ptr_array_unref (self->curves);

  G_OBJECT_CLASS (gtk_contour_parent_class)->finalize (object);
}

static void
gtk_contour_class_init (GtkContourClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_contour_finalize;
}

GtkContour *
gtk_contour_new (const graphene_point_t *start)
{
  GtkContour *self;

  self = g_object_new (GTK_TYPE_CONTOUR, NULL);

  self->start = *start;

  return self;
}

static inline unsigned int
num_curves (GtkContour *self)
{
  return self->curves->len;
}

unsigned int
gtk_contour_get_n_curves (GtkContour *self)
{
  g_return_val_if_fail (GTK_IS_CONTOUR (self), 0);

  return self->curves->len;
}

static inline unsigned int
num_points (GtkContour *self)
{
  return self->curves->len + (1 - self->closed);
}

unsigned int
gtk_contour_get_n_points (GtkContour *self)
{
  g_return_val_if_fail (GTK_IS_CONTOUR (self), 0);

  return num_points (self);
}

void
gtk_contour_set_point (GtkContour             *self,
                       unsigned int            pos,
                       const graphene_point_t *point)
{
  GtkCurve *curve;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (pos < num_points (self));
  g_return_if_fail (point != NULL);

  if (pos < self->curves->len)
    {
      curve = g_ptr_array_index (self->curves, pos);
      curve->p[0] = *point;
    }

  if (pos == 0 && self->closed)
    pos = self->curves->len;

  if (pos > 0)
    {
      curve = g_ptr_array_index (self->curves, pos - 1);
      curve->p[3] = *point;
    }
}

void
gtk_contour_get_point (GtkContour       *self,
                       unsigned int      pos,
                       graphene_point_t *point)

{
  GtkCurve *curve;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (pos < num_points (self));
  g_return_if_fail (point != NULL);

  if (pos < self->curves->len)
    {
      curve = g_ptr_array_index (self->curves, pos);
      *point = curve->p[0];
    }
  else if (pos == self->curves->len)
    {
      curve = g_ptr_array_index (self->curves, pos - 1);
      *point = curve->p[3];
    }
}

void
gtk_contour_get_curve (GtkContour       *self,
                       unsigned int      pos,
                       GskPathOperation *op,
                       graphene_point_t  pts[4],
                       float            *weight)
{
  GtkCurve *curve;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (pos < num_curves (self));

  curve = g_ptr_array_index (self->curves, pos);
  if (op)
    *op = curve->op;
  if (pts)
    memcpy (pts, curve->p, sizeof (graphene_point_t) * 4);
  if (weight)
    *weight = curve->weight;
}

void
gtk_contour_set_curve (GtkContour       *self,
                       unsigned int      pos,
                       GskPathOperation  op,
                       graphene_point_t  p[4],
                       float             weight)
{
  GtkCurve *curve;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (pos < num_curves (self));

  curve = g_ptr_array_index (self->curves, pos);
  curve->op = op;
  memcpy (curve->p, p, sizeof (graphene_point_t) * 4);
  curve->weight = weight;
}

void
gtk_contour_set_line (GtkContour   *self,
                      unsigned int  pos)
{
  GtkCurve *curve;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (pos < num_curves (self));

  curve = g_ptr_array_index (self->curves, pos);
  curve->op = GSK_PATH_LINE;
}

void
gtk_contour_set_quad (GtkContour             *self,
                      unsigned int            pos,
                      const graphene_point_t *cp)
{
  GtkCurve *curve;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (pos < num_curves (self));
  g_return_if_fail (cp != NULL);

  curve = g_ptr_array_index (self->curves, pos);
  curve->op = GSK_PATH_QUAD;
  curve->p[1] = *cp;
}

void
gtk_contour_set_cubic (GtkContour             *self,
                       unsigned int            pos,
                       const graphene_point_t *cp1,
                       const graphene_point_t *cp2)
{
  GtkCurve *curve;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (pos < num_curves (self));
  g_return_if_fail (cp1 != NULL);
  g_return_if_fail (cp2 != NULL);

  curve = g_ptr_array_index (self->curves, pos);
  curve->op = GSK_PATH_CUBIC;
  curve->p[1] = *cp1;
  curve->p[2] = *cp2;
}

void
gtk_contour_set_conic (GtkContour             *self,
                       unsigned int            pos,
                       const graphene_point_t *cp,
                       float                   weight)
{
  GtkCurve *curve;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (pos < num_curves (self));
  g_return_if_fail (cp != NULL);
  g_return_if_fail (0 < weight);

  curve = g_ptr_array_index (self->curves, pos);
  curve->op = GSK_PATH_CUBIC;
  curve->p[1] = *cp;
  curve->weight = weight;
}

static inline const graphene_point_t *
get_start_point (GtkContour *self)
{
  if (self->curves->len > 0)
    {
      GtkCurve *first = g_ptr_array_index (self->curves, 0);
      return &first->p[0];
    }
  else
    {
      return &self->start;
    }
}

static inline const graphene_point_t *
get_end_point (GtkContour *self)
{
  if (self->curves->len > 0)
    {
      GtkCurve *last = g_ptr_array_index (self->curves, self->curves->len - 1);
      return &last->p[3];
    }
  else
    {
      return &self->start;
    }
}

void
gtk_contour_line_to (GtkContour             *self,
                     const graphene_point_t *end)
{
  GtkCurve *curve;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (!self->closed);
  g_return_if_fail (end != NULL);

  curve = g_new0 (GtkCurve, 1);
  curve->op = GSK_PATH_LINE;
  curve->p[0] = *get_end_point (self);
  curve->p[3] = *end;

  g_ptr_array_add (self->curves, curve);
}

void
gtk_contour_quad_to (GtkContour             *self,
                     const graphene_point_t *cp,
                     const graphene_point_t *end)
{
  GtkCurve *curve;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (!self->closed);
  g_return_if_fail (cp != NULL);
  g_return_if_fail (end != NULL);

  curve = g_new0 (GtkCurve, 1);
  curve->op = GSK_PATH_QUAD;
  curve->p[0] = *get_end_point (self);
  curve->p[1] = *cp;
  curve->p[3] = *end;

  g_ptr_array_add (self->curves, curve);
}

void
gtk_contour_cubic_to (GtkContour             *self,
                      const graphene_point_t *cp1,
                      const graphene_point_t *cp2,
                      const graphene_point_t *end)
{
  GtkCurve *curve;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (!self->closed);
  g_return_if_fail (cp1 != NULL);
  g_return_if_fail (cp2 != NULL);
  g_return_if_fail (end != NULL);

  curve = g_new0 (GtkCurve, 1);
  curve->op = GSK_PATH_CUBIC;
  curve->p[0] = *get_end_point (self);
  curve->p[1] = *cp1;
  curve->p[2] = *cp2;
  curve->p[3] = *end;

  g_ptr_array_add (self->curves, curve);
}

void
gtk_contour_conic_to (GtkContour             *self,
                      const graphene_point_t *cp,
                      float                   weight,
                      const graphene_point_t *end)
{
  GtkCurve *curve;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (!self->closed);
  g_return_if_fail (cp != NULL);
  g_return_if_fail (weight > 0);
  g_return_if_fail (end != NULL);

  curve = g_new0 (GtkCurve, 1);
  curve->op = GSK_PATH_CONIC;
  curve->p[0] = *get_end_point (self);
  curve->p[1] = *cp;
  curve->p[3] = *end;
  curve->weight = weight;

  g_ptr_array_add (self->curves, curve);
}

gboolean
gtk_contour_is_closed (GtkContour *self)
{
  g_return_val_if_fail (GTK_IS_CONTOUR (self), FALSE);

  return self->closed;
}

void
gtk_contour_close (GtkContour *self)
{
  g_return_if_fail (GTK_IS_CONTOUR (self));

  if (self->curves->len == 0 || self->closed)
    return;

  if (!graphene_point_near (get_start_point (self), get_end_point (self), 0.01))
    gtk_contour_line_to (self, get_start_point (self));

  self->closed = TRUE;
}

void
gtk_contour_append (GtkContour *self,
                    GtkContour *contour)
{
  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (GTK_IS_CONTOUR (contour));
  g_return_if_fail (!self->closed);
  g_return_if_fail (!contour->closed);

  if (contour->curves->len > 0)
    {
      if (!graphene_point_near (get_end_point (self), get_start_point (contour), 0.01))
        gtk_contour_line_to (self, get_start_point (contour));

      g_ptr_array_extend_and_steal (self->curves, g_ptr_array_ref (contour->curves));
      /* now contour has zero curves left */
    }

  g_object_unref (contour);
}

void
gtk_contour_split (GtkContour     *self,
                   unsigned int    pos,
                   GtkContour    **contour1,
                   GtkContour    **contour2)
{
  GtkCurve *curve, *curve1, *curve2;

  GtkContour *c1, *c2;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (pos < num_curves (self));
  g_return_if_fail (contour1 != NULL);
  g_return_if_fail (contour2 != NULL);

  curve = g_ptr_array_index (self->curves, pos);

  c1 = gtk_contour_new (get_start_point (self));
  c2 = gtk_contour_new (&curve->p[3]);

  for (unsigned int i = 0; i < self->curves->len; i++)
    {
      curve1 = g_ptr_array_index (self->curves, i);

      curve2 = g_new0 (GtkCurve, 1);
      memcpy (curve2, curve1, sizeof (GtkCurve));

      if (i < pos)
        g_ptr_array_add (c1->curves, curve2);
      else if (i > pos)
        g_ptr_array_add (c2->curves, curve2);
    }

  if (self->closed)
    {
      /* Reconnect the pieces */
      gtk_contour_append (c2, c1);

      c1 = c2;
      c2 = NULL;
    }

  *contour1 = c1;
  *contour2 = c2;
}

static inline int
n_points (GskPathOperation op)
{
  switch (op)
    {
    case GSK_PATH_LINE: return 2;
    case GSK_PATH_QUAD: return 3;
    case GSK_PATH_CUBIC: return 4;
    case GSK_PATH_CONIC: return 3;
    case GSK_PATH_MOVE:
    case GSK_PATH_CLOSE:
    default:
      g_assert_not_reached ();
    }
}

static inline void
gtk_curve_init_from_gsk (GtkCurve       *curve,
                         const GskCurve *c)
{
  memset (curve, 0, sizeof (GtkCurve));
  curve->op = c->op;
  curve->weight = 1.f;

  switch (c->op)
    {
    case GSK_PATH_LINE:
      curve->p[0] = c->line.points[0];
      curve->p[3] = c->line.points[1];
      break;
    case GSK_PATH_QUAD:
      curve->p[0] = c->quad.points[0];
      curve->p[1] = c->quad.points[1];
      curve->p[3] = c->quad.points[2];
      break;
    case GSK_PATH_CUBIC:
      curve->p[0] = c->cubic.points[0];
      curve->p[1] = c->cubic.points[1];
      curve->p[2] = c->cubic.points[2];
      curve->p[3] = c->cubic.points[3];
      break;
    case GSK_PATH_CONIC:
      curve->p[0] = c->conic.points[0];
      curve->p[1] = c->conic.points[1];
      curve->p[3] = c->conic.points[3];
      curve->weight = c->conic.points[2].x;
      break;
    case GSK_PATH_MOVE:
    case GSK_PATH_CLOSE:
    default:
      g_assert_not_reached ();
    }
}

static void
split_curve (const GtkCurve *curve,
             float           t,
             GtkCurve        *curve1,
             GtkCurve        *curve2)
{
  GskCurve c, c1, c2;
  graphene_point_t pp[2];
  const graphene_point_t *p = curve->p;

  if (curve->op == GSK_PATH_LINE)
    {
      pp[0] = curve->p[0];
      pp[1] = curve->p[3];
      p = pp;
    }
  else
    p = curve->p;

  gsk_curve_init_foreach (&c, curve->op, p, n_points (curve->op), curve->weight);
  gsk_curve_split (&c, t, &c1, &c2);
  gtk_curve_init_from_gsk (curve1, &c1);
  gtk_curve_init_from_gsk (curve2, &c2);
}

void
gtk_contour_insert_point (GtkContour   *self,
                          unsigned int  pos,
                          float         t)
{
  GtkCurve *curve, *curve2;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (pos < num_curves (self));

  curve = g_ptr_array_index (self->curves, pos);

  curve2 = g_new0 (GtkCurve, 1);
  split_curve (curve, t, curve, curve2);
  g_ptr_array_insert (self->curves, pos + 1, curve2);
}

static int
curve_before_point (GtkContour   *self,
                    unsigned int  point)
{
  if (point > 0)
    return point - 1;
  else if (self->closed)
    return self->curves->len - 1;
  else
    return -1;
}

static int
curve_after_point (GtkContour   *self,
                   unsigned int  point)
{
  if (point < self->curves->len)
    return point;
  else if (self->closed)
    return 0;
  else
    return -1;
}

static gboolean
line_intersection (const graphene_point_t *a,
                   const graphene_point_t *b,
                   const graphene_point_t *c,
                   const graphene_point_t *d,
                   graphene_point_t       *p)
{
  double a1 = b->y - a->y;
  double b1 = a->x - b->x;
  double c1 = a1*a->x + b1*a->y;

  double a2 = d->y - c->y;
  double b2 = c->x - d->x;
  double c2 = a2*c->x+ b2*c->y;

  double det = a1*b2 - a2*b1;

  if (det != 0)
    {
      p->x = (b2*c1 - b1*c2) / det;
      p->y = (a1*c2 - a2*c1) / det;
      return TRUE;
    }

  return FALSE;
}

void
gtk_contour_remove_point (GtkContour   *self,
                          unsigned int pos)
{
  unsigned int before, after;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (pos < num_points (self));

  before = curve_before_point (self, pos);
  after = curve_after_point (self, pos);
  if (before != -1 && after != -1)
    {
      GtkCurve *curve1 = g_ptr_array_index (self->curves, before);
      GtkCurve *curve2 = g_ptr_array_index (self->curves, after);
      graphene_point_t c1, c2;

      if (curve1->op == GSK_PATH_CUBIC && curve2->op == GSK_PATH_CUBIC)
        {
          /* FIXME: We should make the new curve go through
           * the removed point if possible.
           */
          c1 = curve1->p[1];
          c2 = curve2->p[2];
        }
      else
        {
          const graphene_point_t *a, *b, *c, *d;
          graphene_point_t p;

          a = &curve1->p[0];
          d = &curve2->p[3];
          if (curve1->op == GSK_PATH_LINE)
            b = &curve1->p[3];
          else
            b = &curve1->p[1];
          if (curve2->op == GSK_PATH_LINE)
            c = &curve2->p[0];
          else if (curve2->op == GSK_PATH_CUBIC)
            c = &curve2->p[2];
          else
            c = &curve2->p[1];

          if (line_intersection (a, b, c, d, &p))
            {
              graphene_point_interpolate (&curve1->p[0], &p, 0.667, &c1);
              graphene_point_interpolate (&p, &curve2->p[3], 0.333, &c2);
            }
          else
            {
              c1 = *b;
              c2 = *c;
            }
        }

      curve1->op = GSK_PATH_CUBIC;
      curve1->p[1] = c1;
      curve1->p[2] = c2;
      curve1->p[3] = curve2->p[3];

      g_ptr_array_remove_index (self->curves, after);
    }
  else if (before != -1)
    {
      g_ptr_array_remove_index (self->curves, before);
    }
  else if (after != -1)
    {
      g_ptr_array_remove_index (self->curves, after);
    }

  /* Check if the contour is now empty */
  if (self->curves->len == 0)
    self->closed = FALSE;
}

static void
path_builder_add_curve (GskPathBuilder *builder,
                        GtkCurve       *curve)
{
  switch (curve->op)
    {
    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, curve->p[3].x, curve->p[3].y);
      break;

    case GSK_PATH_QUAD:
      gsk_path_builder_quad_to (builder, curve->p[1].x, curve->p[1].y,
                                         curve->p[3].x, curve->p[3].y);
      break;

    case GSK_PATH_CUBIC:
      gsk_path_builder_cubic_to (builder, curve->p[1].x, curve->p[1].y,
                                          curve->p[2].x, curve->p[2].y,
                                          curve->p[3].x, curve->p[3].y);
      break;

    case GSK_PATH_CONIC:
      gsk_path_builder_conic_to (builder, curve->p[1].x, curve->p[1].y,
                                          curve->p[3].x, curve->p[3].y,
                                          curve->weight);
      break;

    case GSK_PATH_MOVE:
    case GSK_PATH_CLOSE:
    default:
      g_assert_not_reached ();
    }
}

gboolean
gtk_contour_find_closest_curve (GtkContour             *self,
                                const graphene_point_t *point,
                                float                   threshold,
                                unsigned int           *idx,
                                graphene_point_t       *p,
                                float                  *pos)
{
  graphene_point_t pp;
  float t;
  unsigned int best;

  g_return_val_if_fail (GTK_IS_CONTOUR (self), FALSE);
  g_return_val_if_fail (point != NULL, FALSE);
  g_return_val_if_fail (threshold > 0, FALSE);

  best = G_MAXUINT;
  for (unsigned int i = 0; i < self->curves->len; i++)
    {
      GtkCurve *curve = g_ptr_array_index (self->curves, i);
      GskPathBuilder *builder;
      GskPath *path;
      GskPathMeasure *measure;
      GskPathPoint *cp;
      float t1;
      graphene_point_t pp1;

      builder = gsk_path_builder_new ();
      gsk_path_builder_move_to (builder, curve->p[0].x, curve->p[0].y);
      path_builder_add_curve (builder, curve);
      path = gsk_path_builder_free_to_path (builder);
      measure = gsk_path_measure_new (path);

      cp = gsk_path_measure_get_closest_point (measure, point, threshold);
      if (cp)
        {
          gsk_path_point_get_position (cp, &pp1);
          t1 = gsk_path_point_get_distance (cp);
          threshold = graphene_point_distance (&pp1, point, NULL, NULL);
          gsk_path_point_unref (cp);

          best = i;
          t = t1 / gsk_path_measure_get_length (measure);
          pp = pp1;
        }

      gsk_path_measure_unref (measure);
      gsk_path_unref (path);
    }

  if (best != G_MAXUINT)
    {
      if (idx)
        *idx = best;
      if (pos)
        *pos = t;
      if (p)
        *p = pp;

      return TRUE;
    }

  return FALSE;
}

void
gtk_contour_add_to_path_builder (GtkContour     *self,
                                 GskPathBuilder *builder)
{
  GtkCurve *curve;

  g_return_if_fail (GTK_IS_CONTOUR (self));
  g_return_if_fail (builder != NULL);

  if (self->curves->len == 0)
    return;

  curve = g_ptr_array_index (self->curves, 0);
  gsk_path_builder_move_to (builder, curve->p[0].x, curve->p[0].y);

  for (unsigned int i = 0; i < self->curves->len; i++)
    {
      curve = g_ptr_array_index (self->curves, i);
      path_builder_add_curve (builder, curve);
    }

  if (self->closed)
    {
      gsk_path_builder_close (builder);
    }
}
