#include "curve-editor.h"

#include <gtk/gtk.h>

#define DRAW_RADIUS 5
#define CLICK_RADIUS 8

/* {{{ Types and structures */
typedef enum
{
  MOVE,
  LINE,
  CURVE
} Operation;

static const char *
op_to_string (Operation op)
{
  switch (op)
    {
    case MOVE:
      return "move";
    case LINE:
      return "line";
    case CURVE:
      return "curve";
    default:
      g_assert_not_reached ();
    }
}

static Operation
op_from_string (const char *s)
{
  if (strcmp (s, "move") == 0)
    return MOVE;
  else if (strcmp (s, "line") == 0)
    return LINE;
  else if (strcmp (s, "curve") == 0)
    return CURVE;
  else
    g_assert_not_reached ();
}

/* We don't store Bezier segments, but an array of points on
 * the line. Each point comes with its two neighboring control
 * points, so each Bezier segment contains p[1] and p[2] from
 * one point, and p[0] and p[1] from the next.
 *
 * The control points are irrelevant for MOVE and LINE segments.
 */
typedef struct
{
  /* 0 and 2 are control points, 1 is the point on the line */
  graphene_point_t p[3];
  gboolean edit;
  gboolean smooth;
  gboolean symmetric;
  int dragged;
  int hovered;
  /* refers to the segment following the point */
  Operation op;
} PointData;

struct _CurveEditor
{
  GtkWidget parent_instance;
  PointData *points;
  int n_points;
  int dragged;
  int context;
  gboolean edit;

  GtkWidget *menu;
  GActionMap *actions;
};

struct _CurveEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (CurveEditor, curve_editor, GTK_TYPE_WIDGET)
/* }}} */
/* {{{ Misc. geometry */
/* Set q to the projection of p onto the line through a and b */
static void
closest_point (const graphene_point_t *p,
               const graphene_point_t *a,
               const graphene_point_t *b,
               graphene_point_t       *q)
{
  graphene_vec2_t n;
  graphene_vec2_t ap;
  float t;

  graphene_vec2_init (&n, b->x - a->x, b->y - a->y);
  graphene_vec2_init (&ap, p->x - a->x, p->y - a->y);

  t = graphene_vec2_dot (&ap, &n) / graphene_vec2_dot (&n, &n);

  q->x = a->x + t * (b->x - a->x);
  q->y = a->y + t * (b->y - a->y);
}

/* Determine if p is on the line through a and b */
static gboolean
collinear (const graphene_point_t *p,
           const graphene_point_t *a,
           const graphene_point_t *b)
{
  graphene_point_t q;

  closest_point (p, a, b, &q);

  return graphene_point_near (p, &q, 0.0001);
}

/* Set q to the point on the line through p and a that is
 * at a distance of d from p, on the opposite side
 */
static void
opposite_point (const graphene_point_t *p,
                const graphene_point_t *a,
                float                   d,
                graphene_point_t       *q)
{
  graphene_vec2_t ap;
  float t;

  graphene_vec2_init (&ap, p->x - a->x, p->y - a->y);
  t = - sqrt (d * d / graphene_vec2_dot (&ap, &ap));

  q->x = p->x + t * (a->x - p->x);
  q->y = p->y + t * (a->y - p->y);
}

/* Set q to the point on the line through p and a that is
 * at a distance of d from p, on the same side
 */
static void
scale_point (const graphene_point_t *p,
             const graphene_point_t *a,
             float                   d,
             graphene_point_t       *q)
{
  graphene_vec2_t ap;
  float t;

  graphene_vec2_init (&ap, p->x - a->x, p->y - a->y);
  t = sqrt (d * d / graphene_vec2_dot (&ap, &ap));

  q->x = p->x + t * (a->x - p->x);
  q->y = p->y + t * (a->y - p->y);
}

/* }}} */
/* {{{ Misc. Bezier math */
static void
find_line_point (graphene_point_t *a,
                 graphene_point_t *b,
                 graphene_point_t *p,
                 double           *t,
                 graphene_point_t *pp,
                 double           *d)
{
  graphene_vec2_t n;
  graphene_vec2_t ap;
  float tt;

  graphene_vec2_init (&n, b->x - a->x, b->y - a->y);
  graphene_vec2_init (&ap, p->x - a->x, p->y - a->y);

  tt = graphene_vec2_dot (&ap, &n) / graphene_vec2_dot (&n, &n);

  if (tt < 0)
    {
      *pp = *a;
      *t = 0;
      *d = graphene_point_distance (a, p, NULL, NULL);
    }
  else if (tt > 1)
    {
      *pp = *b;
      *t = 1;
      *d = graphene_point_distance (b, p, NULL, NULL);
    }
  else
    {
      pp->x = a->x + tt * (b->x - a->x);
      pp->y = a->y + tt * (b->y - a->y);
      *t = tt;
      *d = graphene_point_distance (pp, p, NULL, NULL);
    }
}

static void
gsk_split_get_coefficients (graphene_point_t       coeffs[4],
                            const graphene_point_t pts[4])
{
  coeffs[0] = GRAPHENE_POINT_INIT (pts[3].x - 3.0f * pts[2].x + 3.0f * pts[1].x - pts[0].x,
                                   pts[3].y - 3.0f * pts[2].y + 3.0f * pts[1].y - pts[0].y);
  coeffs[1] = GRAPHENE_POINT_INIT (3.0f * pts[2].x - 6.0f * pts[1].x + 3.0f * pts[0].x,
                                   3.0f * pts[2].y - 6.0f * pts[1].y + 3.0f * pts[0].y);
  coeffs[2] = GRAPHENE_POINT_INIT (3.0f * pts[1].x - 3.0f * pts[0].x,
                                   3.0f * pts[1].y - 3.0f * pts[0].y);
  coeffs[3] = pts[0];
}

static void
gsk_spline_get_point_cubic (const graphene_point_t  pts[4],
                            float                   progress,
                            graphene_point_t       *pos,
                            graphene_vec2_t        *tangent)
{
  graphene_point_t c[4];

  gsk_split_get_coefficients (c, pts);
  if (pos)
    *pos = GRAPHENE_POINT_INIT (((c[0].x * progress + c[1].x) * progress +c[2].x) * progress + c[3].x,
                                ((c[0].y * progress + c[1].y) * progress +c[2].y) * progress + c[3].y);
  if (tangent)
    {
      graphene_vec2_init (tangent,
                          (3.0f * c[0].x * progress + 2.0f * c[1].x) * progress + c[2].x,
                          (3.0f * c[0].y * progress + 2.0f * c[1].y) * progress + c[2].y);
      graphene_vec2_normalize (tangent, tangent);
    }
}

static void
find_curve_point (graphene_point_t *points,
                  graphene_point_t *p,
                  double           *t,
                  graphene_point_t *pp,
                  double           *d)
{
  graphene_point_t q;
  graphene_point_t best_p;
  double best_d;
  double best_t;
  double dd;
  double tt;
  int i;

  best_d = G_MAXDOUBLE;
  best_t = 0;

  for (i = 0; i < 20; i++)
    {
      tt = i / 20.0;
      gsk_spline_get_point_cubic (points, tt, &q, NULL);
      dd = graphene_point_distance (&q, p, NULL, NULL);
      if (dd < best_d)
        {
          best_d = dd;
          best_t = tt;
          best_p = q;
        }
    }

  /* TODO: bisect from here */

  *t = best_t;
  *pp = best_p;
  *d = best_d;
}

static void
find_closest_point (CurveEditor      *self,
                    graphene_point_t *p,
                    int              *point,
                    double           *t,
                    double           *d)
{
  int i;
  int best_i;
  double best_d;
  double best_t;
  double tt;
  double dd;
  graphene_point_t pp;

  best_i = -1;
  best_d = G_MAXDOUBLE;
  best_t = 0;

  for (i = 0; i < self->n_points; i++)
    {
      PointData *pd = &self->points[i];
      PointData *pd1 = &self->points[(i + 1) % self->n_points];

      switch (pd->op)
        {
        case MOVE:
          continue;
        case LINE:
          find_line_point (&pd->p[1], &pd1->p[1], p, &tt, &pp, &dd);
          if (dd < best_d)
            {
              best_i = i;
              best_d = dd;
              best_t = tt;
            }
          break;
        case CURVE:
          {
            graphene_point_t points[4];

            points[0] = pd->p[1];
            points[1] = pd->p[2];
            points[2] = pd1->p[0];
            points[3] = pd1->p[1];

            find_curve_point (points, p, &tt, &pp, &dd);
            if (dd < best_d)
              {
                best_i = i;
                best_d = dd;
                best_t = tt;
              }
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }

  *point = best_i;
  *t = best_t;
  *d = best_d;
}

static void
split_bezier (graphene_point_t *points,
              int               length,
              float             t,
              graphene_point_t *left,
              int              *left_pos,
              graphene_point_t *right,
              int              *right_pos)
{
  if (length == 1)
    {
      left[*left_pos] = points[0];
      (*left_pos)++;
      right[*right_pos] = points[0];
      (*right_pos)++;
    }
  else
    {
      graphene_point_t *newpoints;
      int i;

      newpoints = g_alloca (sizeof (graphene_point_t) * (length - 1));
      for (i = 0; i < length - 1; i++)
        {
          if (i == 0)
            {
              left[*left_pos] = points[i];
              (*left_pos)++;
            }
          if (i == length - 2)
            {
              right[*right_pos] = points[i + 1];
              (*right_pos)++;
            }
          graphene_point_interpolate (&points[i], &points[i+1], t, &newpoints[i]);
        }
      split_bezier (newpoints, length - 1, t, left, left_pos, right, right_pos);
    }
}
/* }}} */
/* {{{ Utilities */
static gboolean
point_is_visible (CurveEditor *self,
                  int          point,
                  int          point1)
{
  g_assert (0 <= point && point < self->n_points);

  if (!self->edit)
    return FALSE;

  switch (point1)
    {
    case 0:
      if (!self->points[point].edit)
        return FALSE;
      else
        return self->points[(point - 1 + self->n_points) % self->n_points].op == CURVE;
    case 1: /* point on curve */
      return TRUE;
    case 2:
      if (!self->points[point].edit)
        return FALSE;
      else
        return self->points[point].op == CURVE;
    default:
      g_assert_not_reached ();
    }
}

static void
maintain_smoothness (CurveEditor *self,
                     int          point)
{
  PointData *pd;
  Operation op, op1;
  graphene_point_t *p;

  pd = &self->points[point];

  if (!pd->smooth)
    return;

  op = pd->op;
  op1 = self->points[(point - 1 + self->n_points) % self->n_points].op;

  p = &pd->p[1];

  if (op == CURVE && op1 == CURVE)
    {
      graphene_point_t *c, *c2;
      float d;

      c = &pd->p[0];
      c2 = &pd->p[2];

      d = graphene_point_distance (c, p, NULL, NULL);
      opposite_point (p, c2, d, c);
    }
  else if (op == CURVE && op1 == LINE)
    {
      graphene_point_t *c, *p2;
      float d;

      c = &pd->p[2];
      p2 = &self->points[(point - 1 + self->n_points) % self->n_points].p[1];

      d = graphene_point_distance (c, p, NULL, NULL);
      opposite_point (p, p2, d, c);
    }
  else if (op == LINE && op1 == CURVE)
    {
      graphene_point_t *c, *p2;
      float d;

      c = &pd->p[0];
      p2 = &self->points[(point + 1) % self->n_points].p[1];

      d = graphene_point_distance (c, p, NULL, NULL);
      opposite_point (p, p2, d, c);
    }
}

static void
maintain_symmetry (CurveEditor *self,
                   int          point)
{
  PointData *pd;
  graphene_point_t *p, *c, *c2;
  double l1, l2, l;

  pd = &self->points[point];

  if (!pd->symmetric)
    return;

  c = &pd->p[0];
  p = &pd->p[1];
  c2 = &pd->p[2];

  l1 = graphene_point_distance (p, c, NULL, NULL);
  l2 = graphene_point_distance (p, c2, NULL, NULL);

  if (l1 != l2)
    {
      l = (l1 + l2) / 2;

      scale_point (p, c, l, c);
      scale_point (p, c2, l, c2);
    }
}

/* Check if the points arount point currently satisfy
 * smoothness conditions. Set PointData.smooth accordingly.
 */
static void
check_smoothness (CurveEditor *self,
                  int          point)
{
  Operation op, op1;
  graphene_point_t *p1, *p2;
  PointData *pd;

  pd = &self->points[point];
  op = pd->op;
  op1 = self->points[(point - 1 + self->n_points) % self->n_points].op;

  if (op == CURVE)
    p2 = &pd->p[2];
  else if (op == LINE)
    p2 = &self->points[(point + 1) % self->n_points].p[1];
  else
    p2 = NULL;

  if (op1 == CURVE)
    p1 = &pd->p[0];
  else if (op1 == LINE)
    p1 = &self->points[(point - 1 + self->n_points) % self->n_points].p[1];
  else
    p1 = NULL;

  if (p1 && p2)
    pd->smooth = collinear (&pd->p[1], p1, p2);
  else
    pd->smooth = TRUE;
}

static void
insert_point (CurveEditor *self,
              int          point,
              double       pos)
{
  Operation op = self->points[point].op;
  int i;
  graphene_point_t points[4];
  int point1, point2;

  if (op == MOVE)
    return;

  point1 = (point + 1) % self->n_points;
  points[0] = self->points[point].p[1];
  points[1] = self->points[point].p[2];
  points[2] = self->points[point1].p[0];
  points[3] = self->points[point1].p[1];

  self->points = g_realloc (self->points, sizeof (PointData) * (self->n_points + 1));
  for (i = self->n_points; i > point; i--)
    self->points[i] = self->points[i - 1];

  self->n_points += 1;

  point1 = (point + 1) % self->n_points;
  point2 = (point + 2) % self->n_points;

  self->points[point1].smooth = TRUE;
  self->points[point1].hovered = -1;

  if (op == LINE)
    {
      graphene_point_t p;

      self->points[point1].op = LINE;

      graphene_point_interpolate (&points[0], &points[3], pos, &p);
      self->points[point1].p[1] = p;
    }
  else if (op == CURVE)
    {
      graphene_point_t left[4];
      graphene_point_t right[4];
      int left_pos = 0;
      int right_pos = 0;

      self->points[point1].op = CURVE;

      split_bezier (points, 4, pos, left, &left_pos, right, &right_pos);

      self->points[point].p[1] = left[0];
      self->points[point].p[2] = left[1];
      self->points[point1].p[0] = left[2];
      self->points[point1].p[1] = left[3];
      self->points[point1].p[2] = right[2];
      self->points[point2].p[0] = right[1];
      self->points[point2].p[1] = right[0];
    }
  else
    g_assert_not_reached ();

  gtk_widget_queue_draw (GTK_WIDGET (self));
}
/* }}} */
/* {{{ GskPath helpers */ 
static void
curve_editor_add_path (CurveEditor    *self,
                       GskPathBuilder *builder)
{
  int i;

  if (self->n_points > 0)
    gsk_path_builder_move_to (builder, self->points[0].p[1].x, self->points[0].p[1].y);

  for (i = 0; i < self->n_points; i++)
    {
      PointData *pd1, *pd;

      pd1 = &self->points[i];
      pd = &self->points[(i + 1) % self->n_points];

      switch (pd1->op)
        {
        case MOVE:
          gsk_path_builder_move_to (builder, pd->p[1].x, pd->p[1].y);
          break;

        case LINE:
          gsk_path_builder_line_to (builder, pd->p[1].x, pd->p[1].y);
          break;

        case CURVE:
          gsk_path_builder_curve_to (builder,
                                     pd1->p[2].x, pd1->p[2].y,
                                     pd->p[0].x, pd->p[0].y,
                                     pd->p[1].x, pd->p[1].y);
          break;

        default:
          g_assert_not_reached ();
        }
    }
}

typedef struct
{
  int count;
  graphene_point_t first;
  graphene_point_t last;
  gboolean has_close;
  gboolean has_initial_move;
} CountSegmentData;

static gboolean
count_segments (GskPathOperation        op,
                const graphene_point_t *pts,
                gsize                   n_pts,
                gpointer                data)
{
  CountSegmentData *d = data;

  if (d->count == 0)
    {
      d->first = pts[0];
      if (op == GSK_PATH_MOVE)
        d->has_initial_move = TRUE;
    }

  d->last = pts[n_pts - 1];
  d->count++;

  if (op == GSK_PATH_CLOSE)
    d->has_close = TRUE;

  return TRUE;
}

typedef struct
{
  CurveEditor *editor;
  int pos;
} CopySegmentData;

static gboolean
copy_segments (GskPathOperation        op,
               const graphene_point_t *pts,
               gsize                   n_pts,
               gpointer                data)
{
  CopySegmentData *d = data;
  CurveEditor *self = d->editor;

  switch (op)
    {
    case GSK_PATH_MOVE:
      if (d->pos != 0)
        {
          d->editor->points[d->pos - 1].op = MOVE;
          d->editor->points[d->pos % self->n_points].p[1] = pts[0];
          d->pos++;
        }
      break;
    case GSK_PATH_CLOSE:
      break;
    case GSK_PATH_LINE:
      d->editor->points[d->pos - 1].op = LINE;
      d->editor->points[d->pos % self->n_points].p[1] = pts[1];
      break;
    case GSK_PATH_CURVE:
      if (d->pos == 0)
        {
          d->editor->points[d->pos].p[1] = pts[0];
          d->pos++;
        }

      d->editor->points[d->pos - 1].op = CURVE;

      d->editor->points[d->pos - 1].p[2] = pts[1];
      d->editor->points[d->pos % self->n_points].p[0] = pts[2];
      d->editor->points[d->pos % self->n_points].p[1] = pts[3];
      d->pos++;
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}
/* }}} */
/* {{{ Drag implementation */
static void
drag_begin (GtkGestureDrag *gesture,
            double          start_x,
            double          start_y,
            CurveEditor     *self)
{
  int i, j;
  graphene_point_t p = GRAPHENE_POINT_INIT (start_x, start_y);

  if (self->edit)
    for (i = 0; i < self->n_points; i++)
      {
        PointData *pd = &self->points[i];

        for (j = 0; j < 3; j++)
          {
            if (graphene_point_distance (&pd->p[j], &p, NULL, NULL) < CLICK_RADIUS)
              {
                if (point_is_visible (self, i, j))
                  {
                    self->dragged = i;
                    pd->dragged = j;
                    gtk_widget_queue_draw (GTK_WIDGET (self));
                  }
                return;
              }
          }
      }

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
}

static void
drag_update (GtkGestureDrag *gesture,
             double          offset_x,
             double          offset_y,
             CurveEditor     *self)
{
  double x, y;
  double dx, dy;
  graphene_point_t *c, *p, *d;
  double l1, l2;
  PointData *pd;

  if (self->dragged == -1)
    return;

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  gtk_gesture_drag_get_start_point (gesture, &x, &y);

  x += offset_x;
  y += offset_y;

  pd = &self->points[self->dragged];
  d = &pd->p[pd->dragged];

  /* before moving the point, record the distances to its neighbors, since
   * we may want to preserve those
   */
  l1 = graphene_point_distance (&pd->p[1], &pd->p[0], NULL, NULL);
  l2 = graphene_point_distance (&pd->p[1], &pd->p[2], NULL, NULL);

  dx = x - d->x;
  dy = y - d->y;

  if (pd->dragged == 1)
    {
      /* dragged point is on curve */

      Operation op, op1, op11, op2;
      PointData *pd1, *pd2;

      /* first move the point itself */
      d->x = x;
      d->y = y;

      /* adjust control points as needed */
      pd1 = &self->points[((self->dragged - 1 + self->n_points) % self->n_points)];
      pd2 = &self->points[((self->dragged + 1) % self->n_points)];

      op = pd->op;
      op1 = pd1->op;
      op2 = pd2->op;

      if (op1 == LINE)
        {
          /* the other endpoint of the line */
          p = &pd1->p[1];

          if (op == CURVE && pd->smooth)
            {
              /* adjust the control point after the line segment */
              opposite_point (d, p, l2, &pd->p[2]);
            }
          else
            {
              pd->p[2].x += dx;
              pd->p[2].y += dy;
            }

          pd->p[0].x += dx;
          pd->p[0].y += dy;

          op11 = self->points[((self->dragged - 2 + self->n_points) % self->n_points)].op;

          if (op11 == CURVE && pd1->smooth)
            {
              double l;

              /* adjust the control point before the line segment */
              l = graphene_point_distance (&pd1->p[0], p, NULL, NULL);
              opposite_point (p, d, l, &pd1->p[0]);
            }
        }

      if (op == LINE)
        {
          /* the other endpoint of the line */
          p = &pd2->p[1];

          if (op1 == CURVE && pd->smooth)
            {
              /* adjust the control point before the line segment */
              opposite_point (d, p, l1, &pd->p[0]);
            }
          else
            {
              pd->p[0].x += dx;
              pd->p[0].y += dy;
            }

          pd->p[2].x += dx;
          pd->p[2].y += dy;

          if (op2 == CURVE && pd2->smooth)
            {
              double l;

              /* adjust the control point after the line segment */
              l = graphene_point_distance (&pd2->p[2], p, NULL, NULL);
              opposite_point (p, d, l, &pd2->p[2]);
            }
        }

      if (op1 != LINE && op != LINE)
        {
          pd->p[0].x += dx;
          pd->p[0].y += dy;
          pd->p[2].x += dx;
          pd->p[2].y += dy;
        }
    }
  else
    {
      /* dragged point is a control point */

      graphene_point_t *p1;
      Operation op, op1;

      if (pd->dragged == 0)
        {
          c = &pd->p[2];
          p = &pd->p[1];

          op = self->points[(self->dragged - 1 + self->n_points) % self->n_points].op;
          op1 = self->points[self->dragged].op;

          p1 = &self->points[(self->dragged + 1) % self->n_points].p[1];
        }
      else if (pd->dragged == 2)
        {
          c = &pd->p[0];
          p = &pd->p[1];

          op = self->points[self->dragged].op;
          op1 = self->points[(self->dragged - 1 + self->n_points) % self->n_points].op;
          p1 = &self->points[(self->dragged - 1 + self->n_points) % self->n_points].p[1];
        }
      else
        g_assert_not_reached ();

      if (op == CURVE && pd->smooth)
        {
          if (op1 == CURVE)
            {
              double l;

              /* first move the point itself */
              d->x = x;
              d->y = y;

              /* then adjust the other control point */
              if (pd->symmetric)
                l = graphene_point_distance (d, p, NULL, NULL);
              else
                l = graphene_point_distance (c, p, NULL, NULL);

              opposite_point (p, d, l, c);
            }
          else if (op1 == LINE)
            {
              graphene_point_t m = GRAPHENE_POINT_INIT (x, y);
              closest_point (&m, p, p1, d);
            }
          else
            {
              d->x = x;
              d->y = y;
            }
        }
      else
        {
          d->x = x;
          d->y = y;
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
drag_end (GtkGestureDrag *gesture,
          double          offset_x,
          double          offset_y,
          CurveEditor     *self)
{
  drag_update (gesture, offset_x, offset_y, self);
  self->dragged = -1;
}
/* }}} */
/* {{{ A ction callbacks */
static void
set_smooth (GSimpleAction *action,
            GVariant      *value,
            gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  self->points[self->context].smooth = g_variant_get_boolean (value);

  maintain_smoothness (self, self->context);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
set_symmetric (GSimpleAction *action,
               GVariant      *value,
               gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  self->points[self->context].symmetric = g_variant_get_boolean (value);

  maintain_symmetry (self, self->context);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
set_operation (GSimpleAction *action,
               GVariant      *value,
               gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);

  self->points[self->context].op = op_from_string (g_variant_get_string (value, NULL));

  maintain_smoothness (self, self->context);
  maintain_smoothness (self, (self->context + 1) % self->n_points);
  maintain_symmetry (self, self->context);
  maintain_symmetry (self, (self->context + 1) % self->n_points);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
remove_point (GSimpleAction *action,
              GVariant      *value,
              gpointer       data)
{
  CurveEditor *self = CURVE_EDITOR (data);
  int i;

  for (i = self->context; i + 1 < self->n_points; i++)
    self->points[i] = self->points[i + 1];

  self->points = g_realloc (self->points, sizeof (PointData) * (self->n_points - 1));

  self->n_points -= 1;

  maintain_smoothness (self, self->context);

  gtk_widget_queue_draw (GTK_WIDGET (self));
 }
/* }}} */
/* {{{ Event handlers */
static void
pressed (GtkGestureClick *gesture,
         int              n_press,
         double           x,
         double           y,
         CurveEditor      *self)
{
  graphene_point_t m = GRAPHENE_POINT_INIT (x, y);
  int i;
  int button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (!self->edit)
    return;

  if (button == GDK_BUTTON_SECONDARY)
    {
      for (i = 0; i < self->n_points; i++)
        {
          PointData *pd = &self->points[i];

          if (graphene_point_distance (&pd->p[1], &m, NULL, NULL) < CLICK_RADIUS)
            {
              GAction *action;

              self->context = i;

              action = g_action_map_lookup_action (self->actions, "smooth");
              g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (pd->smooth));

              action = g_action_map_lookup_action (self->actions, "symmetric");
              if (pd->smooth)
                {
                  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
                  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (pd->symmetric));
                }
              else
                {
                  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
                  g_simple_action_set_state (G_SIMPLE_ACTION (action), FALSE);
                }

              action = g_action_map_lookup_action (self->actions, "operation");

              g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (op_to_string (pd->op)));

              gtk_popover_set_pointing_to (GTK_POPOVER (self->menu),
                                           &(const GdkRectangle){ x, y, 1, 1 });
              gtk_popover_popup (GTK_POPOVER (self->menu));
              return;
            }
        }
    }
}

static void
released (GtkGestureClick *gesture,
          int              n_press,
          double           x,
          double           y,
          CurveEditor      *self)
{
  graphene_point_t m = GRAPHENE_POINT_INIT (x, y);
  int button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  int i;

  if (!self->edit)
    return;

  for (i = 0; i < self->n_points; i++)
    {
      PointData *pd = &self->points[i];

      if (graphene_point_distance (&pd->p[1], &m, NULL, NULL) < CLICK_RADIUS)
        {
          if (button == GDK_BUTTON_PRIMARY)
            {
              pd->edit = !pd->edit;
              gtk_widget_queue_draw (GTK_WIDGET (self));
              return;
            }
        }
    }

  if (button == GDK_BUTTON_PRIMARY)
    {
      int point;
      double t;
      double d;

      find_closest_point (self, &m, &point, &t, &d);

      if (d <= CLICK_RADIUS)
        insert_point (self, point, t);
    }
}

static void
motion (GtkEventControllerMotion *controller,
        double                    x,
        double                    y,
        CurveEditor              *self)
{
  graphene_point_t m = GRAPHENE_POINT_INIT (x, y);
  int i, j;
  gboolean changed = FALSE;

  if (self->edit)
    {
      for (i = 0; i < self->n_points; i++)
        {
          PointData *pd = &self->points[i];
          int hovered = -1;

          for (j = 0; j < 3; j++)
            {
              if (!point_is_visible (self, i, j))
                continue;

              if (graphene_point_distance (&pd->p[j], &m, NULL, NULL) < CLICK_RADIUS)
                {
                  hovered = j;
                  break;
                }
            }
          if (pd->hovered != hovered)
            {
              pd->hovered = hovered;
              changed = TRUE;
            }
        }
    }

  if (changed)
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
leave (GtkEventController *controller,
       CurveEditor        *self)
{
  int i;
  gboolean changed = FALSE;

  for (i = 0; i < self->n_points; i++)
    {
      PointData *pd = &self->points[i];
      if (pd->hovered != -1)
        {
          pd->hovered = -1;
          changed = TRUE;
        }
    }

  if (changed)
    gtk_widget_queue_draw (GTK_WIDGET (self));
}
/* }}} */
/* {{{ Snapshot */
static void
curve_editor_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  CurveEditor *self = (CurveEditor *)widget;
  GskPathBuilder *builder;
  GskPath *path;
  GskStroke *stroke;
  int i, j, k;
  float width;
  float height;

  if (self->n_points == 0)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_width (widget);

  builder = gsk_path_builder_new ();

  if (self->edit)
    {
      /* Add the skeleton */
      for (i = 0; i < self->n_points; i++)
        {
          PointData *pd = &self->points[i];
          gboolean need_move = TRUE;
          if (point_is_visible (self, i, 0))
            {
              gsk_path_builder_move_to (builder, pd->p[0].x, pd->p[0].y);
              gsk_path_builder_line_to (builder, pd->p[1].x, pd->p[1].y);
              need_move = FALSE;
            }
          if (point_is_visible (self, i, 2))
            {
              if (need_move)
                gsk_path_builder_move_to (builder, pd->p[1].x, pd->p[1].y);
              gsk_path_builder_line_to (builder, pd->p[2].x, pd->p[2].y);
            }
        }
    }

  /* Add the curve itself */

  curve_editor_add_path (self, builder);

  /* Stroke everything we have so far */

  path = gsk_path_builder_free_to_path (builder);
  stroke = gsk_stroke_new (1);
  gtk_snapshot_push_stroke (snapshot, path, stroke);
  gsk_stroke_free (stroke);
  gsk_path_unref (path);

  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA){ 0, 0, 0, 1 },
                             &GRAPHENE_RECT_INIT (0, 0, width, height ));

  gtk_snapshot_pop (snapshot);

  if (self->edit)
    {
      /* Draw the circles, in several passes, one for each color */

      const char *colors[] = {
        "white", /* hovered */
        "red",   /* smooth curve points */
        "green", /* sharp curve points */
        "blue"   /* control points */
      };
      GdkRGBA color;

       for (k = 0; k < 4; k++)
        {
          builder = gsk_path_builder_new ();

          for (i = 0; i < self->n_points; i++)
            {
              PointData *pd = &self->points[i];

              for (j = 0; j < 3; j++)
                {
                  switch (k)
                    {
                    case 0:
                      if (j != pd->hovered)
                        continue;
                      break;

                    case 1:
                      if (j == pd->hovered)
                        continue;

                      if (!(j == 1 && pd->smooth))
                        continue;
                      break;

                    case 2:
                      if (j == pd->hovered)
                        continue;

                      if (!(j == 1 && !pd->smooth))
                        continue;
                      break;

                    case 3:
                      if (j == pd->hovered)
                        continue;

                      if (j == 1)
                        continue;

                      if (!point_is_visible (self, i, j))
                        continue;
                      break;

                    default:
                      g_assert_not_reached ();
                    }

                  gsk_path_builder_add_circle (builder, &pd->p[j], DRAW_RADIUS);
                }
            }

          path = gsk_path_builder_free_to_path (builder);

          gtk_snapshot_push_fill (snapshot, path, GSK_FILL_RULE_WINDING);
          gdk_rgba_parse (&color, colors[k]);
          gtk_snapshot_append_color (snapshot, &color, &GRAPHENE_RECT_INIT (0, 0, width, height));
          gtk_snapshot_pop (snapshot);

          stroke = gsk_stroke_new (1.0);
          gtk_snapshot_push_stroke (snapshot, path, stroke);
          gsk_stroke_free (stroke);

          gdk_rgba_parse (&color, "black");
          gtk_snapshot_append_color (snapshot, &color, &GRAPHENE_RECT_INIT (0, 0, width, height));
          gtk_snapshot_pop (snapshot);

          gsk_path_unref (path);
        }
    }
}
/* }}} */
/* {{{ GtkWidget boilerplate */
static void
curve_editor_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum_size,
                     int            *natural_size,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  *minimum_size = 100;
  *natural_size = 200;
}

static void
curve_editor_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  CurveEditor *self = CURVE_EDITOR (widget);

  gtk_native_check_resize (GTK_NATIVE (self->menu));
}
/* }}} */
/* {{{ GObject boilerplate */
static void
curve_editor_dispose (GObject *object)
{
  CurveEditor *self = CURVE_EDITOR (object);

  g_clear_pointer (&self->points, g_free);
  g_clear_pointer (&self->menu, gtk_widget_unparent);

  G_OBJECT_CLASS (curve_editor_parent_class)->dispose (object);
}

static void
curve_editor_class_init (CurveEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = curve_editor_dispose;

  widget_class->snapshot = curve_editor_snapshot;
  widget_class->measure = curve_editor_measure;
  widget_class->size_allocate = curve_editor_size_allocate;
}
/* }}} */
/* {{{ Setup */
static void
curve_editor_init (CurveEditor *self)
{
  GtkEventController *controller;
  GMenu *menu;
  GMenu *section;
  GMenuItem *item;
  GSimpleAction *action;

  self->dragged = -1;
  self->edit = FALSE;

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_drag_new ());
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), GDK_BUTTON_PRIMARY);
  g_signal_connect (controller, "drag-begin", G_CALLBACK (drag_begin), self);
  g_signal_connect (controller, "drag-update", G_CALLBACK (drag_update), self);
  g_signal_connect (controller, "drag-end", G_CALLBACK (drag_end), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
  g_signal_connect (controller, "pressed", G_CALLBACK (pressed), self);
  g_signal_connect (controller, "released", G_CALLBACK (released), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion", G_CALLBACK (motion), self);
  g_signal_connect (controller, "leave", G_CALLBACK (leave), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  self->points = NULL;
  self->n_points = 0;

  self->actions = G_ACTION_MAP (g_simple_action_group_new ());

  action = g_simple_action_new_stateful ("smooth", NULL, g_variant_new_boolean (FALSE));
  g_signal_connect (action, "change-state", G_CALLBACK (set_smooth), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));
  gtk_widget_insert_action_group (GTK_WIDGET (self), "point", G_ACTION_GROUP (self->actions));

  action = g_simple_action_new_stateful ("symmetric", NULL, g_variant_new_boolean (FALSE));
  g_signal_connect (action, "change-state", G_CALLBACK (set_symmetric), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));
  gtk_widget_insert_action_group (GTK_WIDGET (self), "point", G_ACTION_GROUP (self->actions));

  action = g_simple_action_new_stateful ("operation", G_VARIANT_TYPE_STRING, g_variant_new_string ("curve"));
  g_signal_connect (action, "change-state", G_CALLBACK (set_operation), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  action = g_simple_action_new ("remove", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (remove_point), self);
  g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (action));

  gtk_widget_insert_action_group (GTK_WIDGET (self), "point", G_ACTION_GROUP (self->actions));

  menu = g_menu_new ();

  item = g_menu_item_new ("Smooth", "point.smooth");
  g_menu_append_item (menu, item);
  g_object_unref (item);

  item = g_menu_item_new ("Symmetric", "point.symmetric");
  g_menu_append_item (menu, item);
  g_object_unref (item);

  section = g_menu_new ();

  item = g_menu_item_new ("Move", "point.operation::move");
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Line", "point.operation::line");
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ("Curve", "point.operation::curve");
  g_menu_append_item (section, item);
  g_object_unref (item);

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  item = g_menu_item_new ("Remove", "point.remove");
  g_menu_append_item (section, item);
  g_object_unref (item);

  self->menu = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
  g_object_unref (menu);

  gtk_widget_set_parent (self->menu, GTK_WIDGET (self));
}

/* }}} */
/* {{{ API */
GtkWidget *
curve_editor_new (void)
{
  return g_object_new (curve_editor_get_type (), NULL);
}

void
curve_editor_set_edit (CurveEditor *self,
                       gboolean     edit)
{
  int i;

  self->edit = edit;
  if (!self->edit)
    {
      for (i = 0; i < self->n_points; i++)
        {
          self->points[i].edit = FALSE;
          self->points[i].hovered = -1;
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
curve_editor_set_path (CurveEditor *self,
                       GskPath     *path)
{
  CountSegmentData data;
  CopySegmentData data2;
  int i;

  g_clear_pointer (&self->points, g_free);
  self->n_points = 0;

  data.count = 0;
  data.has_close = FALSE;
  gsk_path_foreach (path, count_segments, &data);

  if (data.has_initial_move)
    data.count--;

  if (!graphene_point_near (&data.first, &data.last, 0.0001) && !data.has_close)
    data.count++;

  self->n_points = data.count;
  self->points = g_new0 (PointData, self->n_points);
  for (i = 0; i < self->n_points; i++)
    {
      self->points[i].smooth = FALSE;
      self->points[i].hovered = -1;
    }

  data2.editor = self;
  data2.pos = 0;
  gsk_path_foreach (path, copy_segments, &data2);

  for (i = 0; i < self->n_points; i++)
    check_smoothness (self, i);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

GskPath *
curve_editor_get_path (CurveEditor *self)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();

  curve_editor_add_path (self, builder);

  return gsk_path_builder_free_to_path (builder);
}
/* }}} */
/* vim:set foldmethod=marker expandtab: */
