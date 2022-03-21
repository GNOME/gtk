/*
 * Copyright © 2022 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen
 */

#include "config.h"

#include "gskpathprivate.h"

#include "gskcurveprivate.h"
#include "gskpathbuilder.h"
#include "gskpathmeasure.h"
#include "gskstrokeprivate.h"

#include "gskdebugprivate.h"

#include "gdk/gdk-private.h"

#define RAD_TO_DEG(r) ((r + M_PI)*180.0/M_PI)
#define DEG_TO_RAD(d) ((d)*M_PI/180.0)

/* {{{ General utilities */

static void
g_list_insert_after (GList    *l,
                     gpointer  data)
{
  GList *res G_GNUC_UNUSED;
  /* Silence warn-if-unused */
  res = g_list_insert_before (l, l->next, data);
}

/* }}} */
/* {{{ GskPath utilities */

static GskPath *
gsk_path_remove_unclosed (GskPath *path)
{
  GSList *contours;
  GskPath *path2;

  if (gsk_path_get_flags (path) & GSK_PATH_CLOSED)
    return path;

  contours = NULL;
  for (int i = 0; i < gsk_path_get_n_contours (path); i++)
    {
      const GskContour *contour = gsk_path_get_contour (path, i);
      if (gsk_contour_get_flags (contour) & GSK_PATH_CLOSED)
        contours = g_slist_prepend (contours, (gpointer)contour);
    }

  GSK_NOTE (PATHS, g_print ("%ld contours, %d closed\n", gsk_path_get_n_contours (path), g_slist_length (contours)));

  contours = g_slist_reverse (contours);
  path2 = gsk_path_new_from_contours (contours);
  g_slist_free (contours);

  gsk_path_unref (path);

  return path2;
}

/* }}} */
/* {{{ GskCurve utilities */

static gboolean
curve_is_tiny (GskCurve *curve)
{
  GskBoundingBox bounds;

  gsk_curve_get_bounds (curve, &bounds);

  return bounds.max.x - bounds.min.x < 0.01 &&
         bounds.max.y - bounds.min.y < 0.01;
}

static gboolean
curves_coincide (GskCurve *c1,
                 GskCurve *c2)
{
  graphene_point_t p1, p2;

  if (c1->op != c2->op)
    return FALSE;

  if (c1->op == GSK_PATH_LINE)
    return TRUE;

  gsk_curve_get_point (c1, 0.5, &p1);
  gsk_curve_get_point (c2, 0.5, &p2);

  return graphene_point_near (&p1, &p2, 0.01);
}

/* }}} */
/* {{{ Graph types and helpers */

typedef struct
{
  graphene_point_t p;
  GPtrArray *edges;
  int internal;
  int inconsistent;
  int boundaries;
#ifdef G_ENABLE_DEBUG
  char *name;
#endif
} Node;

static void
node_free (Node *c)
{
  g_ptr_array_free (c->edges, TRUE);
#ifdef G_ENABLE_DEBUG
  g_free (c->name);
#endif
  g_free (c);
}

/* Used to describe what we find to the side of an edge */
typedef enum
{
  AREA_UNKNOWN,
  AREA_IN,
  AREA_OUT,
} AreaClassification;

typedef struct
{
  GskCurve curve;
  Node *start;
  Node *end;
  AreaClassification area_left1;
  AreaClassification area_right1;
  AreaClassification area_left2;
  AreaClassification area_right2;
  AreaClassification area_left;
  AreaClassification area_right;
  gboolean interior;
  gboolean collected;
  float start_angle;
  float end_angle;
  int path_num;
  int curve_num;
  int intersect_next;
  gboolean coincides;
  gboolean remove;
#ifdef G_ENABLE_DEBUG
  char *name;
#endif
} Edge;

static void
edge_free (Edge *c)
{
#ifdef G_ENABLE_DEBUG
  g_free (c->name);
#endif
  g_free (c);
}

static void
reverse_edge (Edge *edge)
{
  GskCurve r;
  Node *s;
  AreaClassification a;
  float angle;

  GSK_NOTE (PATHS, g_print ("reversing %s\n", edge->name));
  gsk_curve_reverse (&edge->curve, &r);
  edge->curve = r;

#define SWAP(a,b,c) a = b; b = c; c = a;

  SWAP (s, edge->start, edge->end);
  SWAP (a, edge->area_left1, edge->area_right1);
  SWAP (a, edge->area_left2, edge->area_right2);
  SWAP (a, edge->area_left, edge->area_right);
  SWAP (angle, edge->start_angle, edge->end_angle);

#undef SWAP
}

static void
merge_nodes (GList **nodes,
             Node   *c1,
             Node   *c2)
{
  g_assert (graphene_point_near (&c1->p, &c2->p, 20));

  if (c1 == c2)
    return;

  for (int i = 0; i < c2->edges->len; i++)
    {
      Edge *edge = g_ptr_array_index (c2->edges, i);
      if (edge->start == c2)
        edge->start = c1;
      if (edge->end == c2)
        edge->end = c1;
      g_ptr_array_add (c1->edges, edge);
    }

  *nodes = g_list_remove (*nodes, c2);

  node_free (c2);
}

/* }}} */
/* {{{ Path Op Data */

typedef struct
{
  GskPathOp operation;
  GskPathMeasure *first;
  GskPathMeasure *second;

  GList *edges;
  GList *nodes;
  Edge *start;
  int curve_num;
  int path_num;
} PathOpData;

/* }}} */
/* {{{ Debugging */

#ifdef G_ENABLE_DEBUG
static void
dump_node (Node *c)
{
  g_print ("%s%s\n", c->name, c->inconsistent ? " BAD" : "");

  for (int i = 0; i < c->edges->len; i++)
    {
      Edge *n = g_ptr_array_index (c->edges, i);
      float angle;
      const char *class = " 10";
      const char *ind1 = "   ";
      const char *ind2 = "";
      char buf[4];

      if (n->area_left != AREA_UNKNOWN && n->area_right != AREA_UNKNOWN)
        {
          if (n->end == c)
            buf[0] = '>';
          else
            buf[0] = '<';
          buf[1] = class[n->area_left];
          buf[2] = class[n->area_right];
          buf[3] = '\0';
          ind1 = buf;
          if (n->interior)
            {
              buf[0] = '[';
              ind2 = "]";
            }
          else if (n->collected)
            {
              buf[0] = '(';
              ind2 = ")";
            }
        }

      angle = n->start == c ? n->start_angle : n->end_angle;

      g_print ("\t%s %s %s %g\n", ind1, n->name, ind2, RAD_TO_DEG (angle));
    }
}

static void
validate_edges (GList *edges)
{
  for (GList *l = edges; l; l = l->next)
    {
      Edge *c = l->data;

      GSK_NOTE (PATHS,
        ({
          const char *class = " 10";
          const char *ind1 = "   ";
          const char *ind2 = "";
          char buf[4];

          if (c->area_left != AREA_UNKNOWN && c->area_right != AREA_UNKNOWN)
            {
              buf[0] = ' ';
              buf[1] = class[c->area_left];
              buf[2] = class[c->area_right];
              buf[3] = '\0';
              ind1 = buf;

              if (c->interior)
                {
                  buf[0] = '[';
                  ind2 = "]";
                }

              if (c->coincides)
                buf[0] = '=';
            }

          g_print ("%s %s: ", ind1, c->name);
          gsk_curve_print (&c->curve);
          g_print (" %s\n", ind2);
        }));

      g_assert (c->curve.op == GSK_PATH_LINE || c->curve.op == GSK_PATH_CURVE);
      g_assert (g_ptr_array_find (c->start->edges, c, NULL));
      g_assert (g_ptr_array_find (c->end->edges, c, NULL));
    }
}

static void
validate_nodes (GList *nodes)
{
  for (GList *l = nodes; l; l = l->next)
    {
      Node *c = l->data;

      for (int i = 0; i < c->edges->len; i++)
        {
          Edge *edge = g_ptr_array_index (c->edges, i);

          g_assert (edge->start == c || edge->end == c);
        }
    }
}

static void
dump_dotfile (GList      *edges,
              GList      *nodes,
              const char *filename)
{
  GString *s;

  s = g_string_new ("");

  g_string_append (s, "digraph {\n");

  for (GList *l = nodes; l; l = l->next)
    {
      Node *c = l->data;

      g_string_append_printf (s, "\"%p\" [label=\"%s\",color=%s]\n", c, c->name, c->inconsistent ? "red" : (c->boundaries == 0 ? "gray" : "black"));
    }

  for (GList *l = edges; l; l = l->next)
    {
      Edge *edge = l->data;

      g_string_append_printf (s, "\"%p\" -> \"%p\" [label=\"%s\",color=%s]\n", edge->start, edge->end, edge->name, edge->interior ? "gray" : "black");
    }

  g_string_append (s, "}\n");

  g_file_set_contents (filename, s->str, s->len, NULL);
  g_string_free (s, TRUE);
}
#endif

/* }}} */
/* {{{ Collection helpers */

static gboolean
collect_cb (GskPathOperation        op,
            const graphene_point_t *pts,
            gsize                   n_pts,
            float                   weight,
            gpointer                user_data)
{
  PathOpData *opdata = user_data;
  Edge *edge;
  Node *node;

  g_assert (op != GSK_PATH_CONIC);

  if (op == GSK_PATH_MOVE)
    return TRUE;

  if (op == GSK_PATH_CLOSE)
    {
      Edge *prev;

      if (!graphene_point_near (&pts[0], &pts[1], 0.01))
        collect_cb (GSK_PATH_LINE, pts, n_pts, weight, user_data);

      prev = opdata->edges->data;
      if (opdata->start) /* Ignore M followed by Z */
        merge_nodes (&opdata->nodes, opdata->start->start, prev->end);
      opdata->start = NULL;
      return TRUE;
    }

  edge = g_new0 (Edge, 1);
  gsk_curve_init (&edge->curve, gsk_pathop_encode (op, pts));

  if (opdata->start)
    {
      Edge *prev = opdata->edges->data;

      edge->start = prev->end;
    }
  else
    {
      opdata->start = edge;

      node = g_new0 (Node, 1);
      node->p = *gsk_curve_get_start_point (&edge->curve);
      node->edges = g_ptr_array_new ();
#ifdef G_ENABLE_DEBUG
      node->name = g_strdup_printf ("start %d", opdata->curve_num);
#endif

      opdata->nodes = g_list_prepend (opdata->nodes, node);

      edge->start = node;
    }

  node = g_new0 (Node, 1);
  node->p = *gsk_curve_get_end_point (&edge->curve);
  node->edges = g_ptr_array_new ();
#ifdef G_ENABLE_DEBUG
  node->name = g_strdup_printf ("end %d", opdata->curve_num);
#endif

  opdata->nodes = g_list_prepend (opdata->nodes, node);

  edge->end = node;

  g_ptr_array_add (edge->start->edges, edge);
  g_ptr_array_add (edge->end->edges, edge);

  opdata->edges = g_list_prepend (opdata->edges, edge);

#ifdef G_ENABLE_DEBUG
  {
    const char *opname[] = { "Move", "Close", "Line", "Curve", "Conic" };
    edge->name = g_strdup_printf ("%s %d", opname[op], opdata->curve_num);
  }
#endif

  edge->curve_num = opdata->curve_num++;
  edge->path_num = opdata->path_num;

  return TRUE;
}

static void
collect_edges (GskPath    *path,
               PathOpData *opdata)
{
  for (int i = 0; i < gsk_path_get_n_contours (path); i++)
    {
      const GskContour *contour = gsk_path_get_contour (path, i);

      if (gsk_contour_get_flags (contour) & GSK_PATH_CLOSED)
        gsk_contour_foreach (contour, GSK_PATH_TOLERANCE_DEFAULT, collect_cb, opdata);
    }

  opdata->path_num++;
}

/* }}} */
/* {{{ Splitting helpers */
#define NEAR(f1, f2) (fabs ((f2) - (f1)) < 0.005)

typedef struct
{
  float t1;
  float t2;
  graphene_point_t p;
  Node *node;
} SplitPoint;

static int
compare_t1 (const void *p1,
            const void *p2)
{
  const SplitPoint *sp1 = p1;
  const SplitPoint *sp2 = p2;
  return sp1->t1 < sp2->t1 ? -1 : (sp1->t1 > sp2->t1 ? 1 : 0);
}

static int
compare_t2 (const void *p1,
            const void *p2)
{
  const SplitPoint *sp1 = p1;
  const SplitPoint *sp2 = p2;
  return sp1->t2 < sp2->t2 ? -1 : (sp1->t2 > sp2->t2 ? 1 : 0);
}

/* }}} */
/* {{{ Classification helpers */

static void
compute_coincidence (Edge *edge)
{
  if (edge->coincides || edge->remove)
    return;

  for (int i = 0; i < edge->start->edges->len; i++)
    {
      Edge *other = g_ptr_array_index (edge->start->edges, i);

      if (other->remove)
        continue;

      if (other != edge &&
          other->curve.op == edge->curve.op &&
          other->path_num != edge->path_num &&
          g_ptr_array_find (edge->end->edges, other, NULL) &&
          curves_coincide (&edge->curve, &other->curve))
        {
          edge->coincides = TRUE;
          other->remove = TRUE;
          break;
        }
    }
}

static inline AreaClassification
apply_op (GskPathOp          op,
          AreaClassification c1,
          AreaClassification c2)
{
  switch (op)
    {
    case GSK_PATH_OP_SIMPLIFY:     return c1;
    case GSK_PATH_OP_UNION:        return (c1 == AREA_IN) || (c2 == AREA_IN) ? AREA_IN : AREA_OUT;
    case GSK_PATH_OP_INTERSECTION: return (c1 == AREA_IN)  && (c2 == AREA_IN) ? AREA_IN : AREA_OUT;
    case GSK_PATH_OP_DIFFERENCE:   return (c1 == AREA_IN) && (c2 == AREA_OUT) ? AREA_IN : AREA_OUT;
    case GSK_PATH_OP_XOR:          return c1 != c2 ? AREA_IN : AREA_OUT;
    default: g_assert_not_reached ();
    }
}

static void
classify_boundary (Edge      *edge,
                   PathOpData *opdata)
{
  graphene_point_t pos, pos1, pos2;
  graphene_vec2_t normal;

  gsk_curve_get_point (&edge->curve, 0.5, &pos);
  gsk_curve_get_normal (&edge->curve, 0.5, &normal);

  pos1.x = pos.x - 0.5 * graphene_vec2_get_x (&normal);
  pos1.y = pos.y - 0.5 * graphene_vec2_get_y (&normal);
  pos2.x = pos.x + 0.5 * graphene_vec2_get_x (&normal);
  pos2.y = pos.y + 0.5 * graphene_vec2_get_y (&normal);

  if (edge->path_num == 1 && !edge->coincides)
    {
      /* Classifying wrt to the other path; check the point
       * on the curve, which is safe since we're already
       * intersected. The only case we need to avoid is if
       * this edge coincides with an edge on the other path.
       */
      edge->area_left1 = edge->area_right1 =
        gsk_path_measure_in_fill (opdata->first, &pos, GSK_FILL_RULE_WINDING) ? AREA_IN : AREA_OUT;
    }
  else
    {
      if (edge->area_left1 == AREA_UNKNOWN)
        edge->area_left1 = gsk_path_measure_in_fill (opdata->first, &pos1, GSK_FILL_RULE_WINDING) ? AREA_IN : AREA_OUT;
      if (edge->area_right1 == AREA_UNKNOWN)
        edge->area_right1 = gsk_path_measure_in_fill (opdata->first, &pos2, GSK_FILL_RULE_WINDING) ? AREA_IN : AREA_OUT;
    }

  if (opdata->second)
    {
      if (edge->path_num == 0 && !edge->coincides)
        {
          /* Classifying wrt to the other path, see above */
          edge->area_left2 = edge->area_right2 =
            gsk_path_measure_in_fill (opdata->second, &pos, GSK_FILL_RULE_WINDING) ? AREA_IN : AREA_OUT;
        }
      else
        {
          if (edge->area_left2 == AREA_UNKNOWN)
            edge->area_left2 = gsk_path_measure_in_fill (opdata->second, &pos1, GSK_FILL_RULE_WINDING) ? AREA_IN : AREA_OUT;
          if (edge->area_right2 == AREA_UNKNOWN)
            edge->area_right2 = gsk_path_measure_in_fill (opdata->second, &pos2, GSK_FILL_RULE_WINDING) ? AREA_IN : AREA_OUT;
        }
    }

  edge->area_left = apply_op (opdata->operation, edge->area_left1, edge->area_left2);
  edge->area_right = apply_op (opdata->operation, edge->area_right1, edge->area_right2);

  edge->interior = edge->area_left == edge->area_right;
}

static inline void
copy_classification (Edge *from,
                     Edge *to)
{
  to->area_left1 = from->area_left1;
  to->area_right1 = from->area_right1;
  to->area_left2 = from->area_left2;
  to->area_right2 = from->area_right2;
  to->area_left = from->area_left;
  to->area_right = from->area_right;
  to->interior = from->interior;
}

static void
propagate_classification_dir (Edge     *edge,
                              gboolean  forward)
{
  Node *c = forward ? edge->end : edge->start;
  Edge *other_edge;
  guint idx;

  if (c->edges->len != 2)
    return;

  g_ptr_array_find (c->edges, edge, &idx);
  other_edge = g_ptr_array_index (c->edges, 1 - idx);

  if (other_edge->area_left == AREA_UNKNOWN ||
      other_edge->area_right == AREA_UNKNOWN)
    {
      copy_classification (edge, other_edge);
      propagate_classification_dir (other_edge, forward);
    }
}

static void
propagate_classification (Edge *edge)
{
  propagate_classification_dir (edge, FALSE);
  propagate_classification_dir (edge, TRUE);
}

static void
compute_angles (Edge *edge)
{
  graphene_vec2_t tangent;

  gsk_curve_get_start_tangent (&edge->curve, &tangent);
  edge->start_angle = atan2 (- graphene_vec2_get_y (&tangent),
                             graphene_vec2_get_x (&tangent));
  gsk_curve_get_end_tangent (&edge->curve, &tangent);
  graphene_vec2_negate (&tangent, &tangent);
  edge->end_angle = atan2 (- graphene_vec2_get_y (&tangent),
                           graphene_vec2_get_x (&tangent));
}

/* }}} */
/* {{{ Consistency helpers */

static gboolean
graph_has_inconsistencies (PathOpData *opdata)
{
  for (GList *l = opdata->nodes; l; l = l->next)
    {
      Node *c = l->data;

      if (c->inconsistent)
        return TRUE;
    }

  return FALSE;
}

static int
compare_angle (gconstpointer p1,
               gconstpointer p2,
               gpointer      user_data)
{
  Node *c = user_data;
  Edge *c1 = *(Edge **)p1;
  Edge *c2 = *(Edge **)p2;
  float f1 = c1->start == c ? c1->start_angle : c1->end_angle;
  float f2 = c2->start == c ? c2->start_angle : c2->end_angle;

  if (fabs (fmod (f1 - f2, 2 * G_PI)) < 0.01)
    {
      graphene_point_t p;
      const graphene_point_t *q;

      if (c1->curve.op == GSK_PATH_CURVE)
        {
          if (c1->start == c)
            {
              gsk_curve_get_point (&c1->curve, 0.333, &p);
              q = gsk_curve_get_start_point (&c1->curve);
            }
          else
            {
              gsk_curve_get_point (&c1->curve, 0.666, &p);
              q = gsk_curve_get_end_point (&c1->curve);
            }
          f1 = atan2 (- (p.y - q->y), p.x - q->x);
        }

      if (c2->curve.op == GSK_PATH_CURVE)
        {
          if (c2->start == c)
            {
              gsk_curve_get_point (&c2->curve, 0.333, &p);
              q = gsk_curve_get_start_point (&c2->curve);
            }
          else
            {
              gsk_curve_get_point (&c2->curve, 0.666, &p);
              q = gsk_curve_get_end_point (&c2->curve);
            }
          f2 = atan2 (- (p.y - q->y), p.x - q->x);
        }
    }

  return f1 < f2 ? -1 : (f1 > f2 ? 1 : 0);
}

static void
check_consistency (Node *c)
{
  int first;
  AreaClassification area, left, right;

  c->boundaries = 0;
  first = -1;
  for (int i = 0; i < c->edges->len; i++)
    {
      Edge *edge = g_ptr_array_index (c->edges, i);

      if (edge->interior)
        continue;

      c->boundaries++;

      if (first == -1)
        {
          first = i;
          if (c == edge->end)
            area = edge->area_right;
          else
            area = edge->area_left;
        }
    }

  if (c->boundaries % 2 != 0)
    {
      c->inconsistent = TRUE;
      return;
    }

  if (c->boundaries == 0)
    return;

  for (int i = 0; i < c->edges->len; i++)
    {
      int pos = (first + i + 1) % (int)c->edges->len;
      Edge *edge = g_ptr_array_index (c->edges, pos);

      if (edge->interior)
        continue;

      if (c == edge->end)
        {
          left = edge->area_left;
          right = edge->area_right;
        }
      else
        {
          left = edge->area_right;
          right = edge->area_left;
        }

      if (left != area)
        {
          c->inconsistent = TRUE;
          return;
        }

      area = right;
    }
}

static gboolean
nodes_share_edge (Node *n1,
                  Node *n2)
{
  for (int i = 0; i < n1->edges->len; i++)
    {
      Edge *edge = g_ptr_array_index (n1->edges, i);

      if (edge->start == n2 || edge->end == n2)
        return TRUE;
    }

  return FALSE;
}

static void
apply_fixups (PathOpData *opdata)
{
  GList *bad = NULL;

  for (GList *l = opdata->nodes; l; l = l->next)
    {
      Node *c = l->data;

      if (c->inconsistent)
        bad = g_list_prepend (bad, c);
    }

  if (bad == NULL)
    return;

  GSK_NOTE (PATHS, g_print ("found %d bad nodes\n", g_list_length (bad)));

  while (bad != NULL)
    {
      Node *n1 = bad->data;
      Node *n2 = NULL;
      gboolean toggle_boundary = FALSE;
      gboolean toggle_area = FALSE;

      bad = g_list_remove (bad, n1);

      for (int i = 0; i < n1->edges->len; i++)
        {
          Edge *edge = g_ptr_array_index (n1->edges, i);

          if (edge->curve.op == GSK_PATH_LINE &&
              !edge->interior &&
              edge->start == edge->end)
            {
              g_ptr_array_remove (edge->start->edges, edge);
              g_ptr_array_remove (edge->end->edges, edge);
              opdata->edges = g_list_remove (opdata->edges, edge);
              edge_free (edge);
              check_consistency (n1);
              if (!n1->inconsistent)
                break;
            }
        }

      if (!n1->inconsistent)
        continue;

      for (GList *l = bad; l; l = l->next)
        {
          if (nodes_share_edge (n1, (Node *)l->data))
            {
              n2 = l->data;
              bad = g_list_remove (bad, n2);
              break;
            }
        }

      if (n2 == NULL)
        continue;

      /* If we have two neighboring nodes with an odd
       * boundary count, we can try to fix it by toggling
       * one of the connecting edges.
       */

      if (n1->boundaries % 2 && n2->boundaries % 2)
        {
          GSK_NOTE (PATHS, g_print ("found 2 odd boundary nodes\n"));
          toggle_boundary = TRUE;
        }

      if ((n1->boundaries % 2) == 0 && (n2->boundaries % 2) == 0)
        {
          GSK_NOTE (PATHS, g_print ("found 2 misordered nodes\n"));
          toggle_area = TRUE;
        }

      for (int i = 0; i < n1->edges->len; i++)
        {
          Edge *edge = g_ptr_array_index (n1->edges, i);

          if (edge->start == n2 || edge->end == n2)
            {
              if (toggle_boundary)
                {
                  GSK_NOTE (PATHS, g_print ("toggling %s from %s to %s\n",
                                            edge->name,
                                            edge->interior ? "interior" : "boundary",
                                            edge->interior ? "boundary" : "interior"));
                  edge->interior = !edge->interior;
                  n1->inconsistent = n2->inconsistent = FALSE;
                  break;
                }
              if (toggle_area && !edge->interior)
                {
                  AreaClassification a;

                  GSK_NOTE (PATHS, g_print ("toggling %s from %d%d to %d%d\n",
                                            edge->name,
                                            edge->area_left == AREA_IN ? 1 : 0,
                                            edge->area_right == AREA_IN ? 1 : 0,
                                            edge->area_left == AREA_IN ? 0 : 1,
                                            edge->area_right == AREA_IN ? 0 : 1));
                  a = edge->area_left;
                  edge->area_left = edge->area_right;
                  edge->area_right = a;

                  check_consistency (n1);
                  check_consistency (n2);

                  if (!n1->inconsistent && !n2->inconsistent)
                    break;
                }
            }
        }
    }
}

/* }}} */
/* {{{ Reassembly helpers */

static Edge *
find_next (Edge *edge)
{
  Node *c = edge->end;
  Edge *next = NULL;
  Edge *next_fallback = NULL;
  guint idx;
  guint dir;

  g_assert (c != NULL);
  g_assert (c->edges != NULL);

  GSK_NOTE (PATHS,
    g_print ("%s ends at: ", edge->name);
    dump_node (edge->end);
  );

  /* Edges are sorted counterclockwise by their tangent.
   * We pick the next eligible edge to the left
   * or to the right of curve, depending on whether
   * the left or right is inside.
   */
  g_ptr_array_find (c->edges, edge, &idx);
  dir = edge->area_left == AREA_IN ? c->edges->len - 1 : 1;

  GSK_NOTE (PATHS, g_print ("picking %s\n", edge->area_left == AREA_IN ? "cw" : "ccw"));

  for (int d = 0; d < c->edges->len; d++)
    {
      guint pos = (guint)((idx + dir * (d + 1)) % c->edges->len);
      Edge *n = g_ptr_array_index (c->edges, pos);
      float angle;

      if (n->collected)
        continue;

      if (n->interior)
        continue;

      angle = n->end == c ? n->end_angle : n->start_angle;
      if (fabs (angle - edge->end_angle) < 0.0001)
        {
          next_fallback = n;
          continue;
        }

      next = n;
      break;
    }

  if (next == NULL)
    next = next_fallback;

  if (next && next->end == c)
    reverse_edge (next);

  return next;
}

/* }}} */
/* {{{ Pathops implementation */

/*
 * The general plan of operation is as follows:
 *
 * 1. We collect all the edges in a list.
 *
 * 2. We add all intersections, splitting the edges as needed,
 *    and keep Node structs that record which edges are meeting
 *    at which intersections. Remove coinciding edges.
 *
 * 3. We classify each edge as boundary or not. This is where
 *    the different boolean ops differ from each other.
 *
 * 4. Sort the edges at each node, counterclockwise.
 *
 * 5. Fix up the resulting graph.
 *
 * 6. Walk the graph, taking the proper turns at each node, to
 *    reassemble contours. Continue doing so until all boundary
 *    edges have been added to a contour.
 *
 * Note that mistakes can happen in various stages, due to
 * rounding errors and approximations (e.g. GskPathMeasure is
 * using linear approximation to determine whether a point is
 * inside).
 *
 * We try to identify places where our graph is inconsistent by
 * checking some invariants:
 * a) At every node, an even number of boundary edges must meet
 * b) Neighboring edges of a node must agree on the area between them
 *
 * We apply some heuristic fixes to patch up these inconsistencies.
 */
GskPath *
gsk_path_op (GskPathOp  operation,
             GskPath   *first,
             GskPath   *second)
{
  GskPathBuilder *builder;
  GList *l, *ll;
  PathOpData opdata;

  GSK_NOTE (PATHS, g_print ("collecting\n"));

  opdata.operation = operation;
  opdata.first = gsk_path_measure_new (first);
  opdata.second = second ? gsk_path_measure_new (second) : NULL;
  opdata.edges = NULL;
  opdata.nodes = NULL;
  opdata.start = NULL;
  opdata.path_num = 0;
  opdata.curve_num = 0;

  collect_edges (first, &opdata);
  if (second)
    collect_edges (second, &opdata);

  opdata.edges = g_list_reverse (opdata.edges);
  opdata.nodes = g_list_reverse (opdata.nodes);

#ifdef G_ENABLE_DEBUG
  validate_edges (opdata.edges);
#endif

  GSK_NOTE (PATHS, g_print ("splitting\n"));
  l = opdata.edges;
  while (l)
    {
      Edge *cd1 = l->data;

      while (l && curve_is_tiny (&cd1->curve))
        {
          l = l->next;
          if (!l)
            break;
          cd1 = l->data;
        }

      if (!l)
        break;

      ll = l;
      while (ll)
        {
          Edge *cd2;
          float t1[9], t2[9];
          graphene_point_t p[9];
          SplitPoint sp[9];
          int n;
          GskCurve cs, ce;
          Edge *cd;
          GList *before;
#ifdef G_ENABLE_DEBUG
          char *name;
#endif
          int path_num1;
          int path_num2;
          int curve_num1;
          int curve_num2;

          cd1 = l->data;
          cd2 = ll->data;

          while (ll && (curve_is_tiny (&cd2->curve) || cd2->curve_num <= cd1->intersect_next))
            {
              ll = ll->next;
              if (!ll)
                break;
              cd2 = ll->data;
            }

          if (!ll)
            break;

          if (cd1->curve.op == GSK_PATH_LINE && cd1->curve_num == cd2->curve_num)
            {
              /* Two segments of the same original line won't intersect */
              ll = ll->next;
              continue;
            }

          path_num1 = cd1->path_num;
          path_num2 = cd2->path_num;
          curve_num1 = cd1->curve_num;
          curve_num2 = cd2->curve_num;

          n = gsk_curve_intersect (&cd1->curve, &cd2->curve, t1, t2, p, G_N_ELEMENTS (t1));

          GSK_NOTE (PATHS,
            g_print ("%d intersections between %s and %s\n", n, cd1->name, cd2->name);
          );

          g_assert (n <= 9);

          if (n == 1)
            {
              if (cd1->start == cd2->start || cd1->start == cd2->end ||
                  cd1->end == cd2->start || cd1->end == cd2->end)
                {
                  /* We already got this one, move on */
                  ll = ll->next;
                  continue;
                }
            }

          for (int i = 0; i < n; i++)
            sp[i] = (SplitPoint) { t1[i], t2[i], p[i], NULL};

          qsort (sp, n, sizeof (SplitPoint), compare_t1);

#ifdef G_ENABLE_DEBUG
          name = g_strdup (cd1->name);
#endif
          before = l;
          for (int i = 0; i < n; i++)
            {
              if (NEAR (sp[i].t1, 0))
                sp[i].node = cd1->start;
              else if (NEAR (sp[i].t1, 1))
                sp[i].node = cd1->end;
              else
                {
                  sp[i].node = g_new0 (Node, 1);
                  sp[i].node->p = sp[i].p;
                  sp[i].node->edges = g_ptr_array_new ();

                  opdata.nodes = g_list_prepend (opdata.nodes, sp[i].node);

                  gsk_curve_split (&cd1->curve, sp[i].t1, &cs, &ce);
                  cd1->curve = cs;
                  cd1->intersect_next = curve_num2;

                  cd = g_new0 (Edge, 1);
                  cd->area_left1 = cd1->area_left1;
                  cd->area_right1 = cd1->area_right1;
                  cd->area_left2 = cd1->area_left2;
                  cd->area_right2 = cd1->area_right2;
                  cd->path_num = path_num1;
                  cd->curve_num = curve_num1;
                  cd->intersect_next = curve_num2;
                  cd->curve = ce;
                  cd->end = cd1->end;
                  g_ptr_array_remove (cd1->end->edges, cd1);
                  g_ptr_array_add (cd->end->edges, cd);

#ifdef G_ENABLE_DEBUG
                  sp[i].node->name = g_strdup_printf ("split %s/%s", name, cd2->name);
                  if (i == 0)
                    {
                      char *nn = g_strdup_printf ("%s.0", name);
                      g_free (cd1->name);
                      cd1->name = nn;
                    }
                  cd->name = g_strdup_printf ("%s.%d", name, i + 1);
#endif

                  GSK_NOTE (PATHS,
                    if (i == 0)
                      {
                        g_print ("split %s from %s at %g\n", cd1->name, name, sp[i].t1);
                        g_print ("%s: ", cd1->name);
                        gsk_curve_print (&cd1->curve);
                        g_print ("\n");
                      }
                    g_print ("split %s from %s at %g\n", cd->name, name, sp[i].t1);
                    g_print ("%s: ", cd->name);
                    gsk_curve_print (&cd->curve);
                    g_print ("\n");
                  );

                  cd1->end = sp[i].node;
                  cd->start = sp[i].node;

                  g_ptr_array_add (cd1->end->edges, cd1);
                  g_ptr_array_add (cd->start->edges, cd);

                  g_list_insert_after (before, cd);
                  before = before->next;

                  cd1 = cd;
                  for (int j = i + 1; j < n; j++)
                    sp[j].t1 = (sp[j].t1 - sp[i].t1) / (1 - sp[i].t1);
                }
            }
#ifdef G_ENABLE_DEBUG
          g_free (name);
#endif

          qsort (sp, n, sizeof (SplitPoint), compare_t2);

#ifdef G_ENABLE_DEBUG
          name = g_strdup (cd2->name);
#endif
          for (int i = 0; i < n; i++)
            {
              if (NEAR (sp[i].t2, 0))
                merge_nodes (&opdata.nodes, sp[i].node, cd2->start);
              else if (NEAR (sp[i].t2, 1))
                merge_nodes (&opdata.nodes, sp[i].node, cd2->end);
              else
                {
                  gsk_curve_split (&cd2->curve, sp[i].t2, &cs, &ce);
                  cd2->curve = cs;
                  cd2->intersect_next = curve_num1;

                  cd = g_new0 (Edge, 1);
                  cd->area_left1 = cd2->area_left1;
                  cd->area_right1 = cd2->area_right1;
                  cd->area_left2 = cd2->area_left2;
                  cd->area_right2 = cd2->area_right2;
                  cd->path_num = path_num2;
                  cd->curve_num = curve_num2;
                  cd->intersect_next = curve_num1;
                  cd->curve = ce;
                  cd->end = cd2->end;
                  g_ptr_array_remove (cd2->end->edges, cd2);
                  g_ptr_array_add (cd->end->edges, cd);

#ifdef G_ENABLE_DEBUG
                  if (i == 0)
                    {
                      char *nn = g_strdup_printf ("%s.0", name);
                      g_free (cd2->name);
                      cd2->name = nn;
                    }
                  cd->name = g_strdup_printf ("%s.%d", name, i + 1);
#endif

                  GSK_NOTE (PATHS,
                    if (i == 0)
                      {
                        g_print ("split %s from %s at %g\n", cd2->name, name, sp[i].t2);
                        g_print ("%s: ", cd2->name);
                        gsk_curve_print (&cd2->curve);
                        g_print ("\n");
                      }
                    g_print ("split %s from %s at %g\n", cd->name, name, sp[i].t2);
                    g_print ("%s: ", cd->name);
                    gsk_curve_print (&cd->curve);
                    g_print ("\n");
                  );

                  cd2->end = sp[i].node;
                  cd->start = sp[i].node;

                  g_ptr_array_add (cd2->end->edges, cd2);
                  g_ptr_array_add (cd->start->edges, cd);

                  g_list_insert_after (ll, cd);
                  ll = ll->next;

                  cd2 = cd;
                  for (int j = i + 1; j < n; j++)
                    sp[j].t2 = (sp[j].t2 - sp[i].t2) / (1 - sp[i].t2);
                }
            }
#ifdef G_ENABLE_DEBUG
          g_free (name);
#endif

          ll = ll->next;
        }

      l = l->next;
    }

  GSK_NOTE (PATHS, g_print ("classifying\n"));

  for (l = opdata.edges; l; l = l->next)
    {
      Edge *edge = l->data;

      if (edge->remove)
        continue;

      compute_coincidence (edge);
      compute_angles (edge);

      classify_boundary (edge, &opdata);
      propagate_classification (edge);
    }

  l = opdata.edges;
  while (l)
    {
      Edge *edge = l->data;
      GList *next = l->next;

      if (edge->remove)
        {
          g_ptr_array_remove (edge->start->edges, edge);
          g_ptr_array_remove (edge->end->edges, edge);
          opdata.edges = g_list_remove (opdata.edges, edge);
          edge_free (edge);
        }

      l = next;
    }

#ifdef G_ENABLE_DEBUG
  validate_edges (opdata.edges);
#endif

  GSK_NOTE (PATHS, g_print ("sorting\n"));

  for (l = opdata.nodes; l; l = l->next)
    {
      Node *c = l->data;

      g_ptr_array_sort_with_data (c->edges, compare_angle, c);
      check_consistency (c);
    }

#ifdef G_ENABLE_DEBUG
  validate_nodes (opdata.nodes);
#endif

  if (graph_has_inconsistencies (&opdata))
    {
      GSK_NOTE (PATHS, dump_dotfile (opdata.edges, opdata.nodes, "inconsistent.dot"));

      GSK_NOTE (PATHS, g_print ("fixups\n"));

      apply_fixups (&opdata);

      GSK_NOTE (PATHS, dump_dotfile (opdata.edges, opdata.nodes, "after-fixups.dot"));
    }

  GSK_NOTE (PATHS, g_print ("reassembling\n"));

  builder = gsk_path_builder_new ();

  for (l = opdata.edges; l; l = l->next)
    {
      Edge *edge = l->data;
      Node *start;

      if (edge->collected || edge->interior)
        continue;

      GSK_NOTE (PATHS, g_print ("start new contour %s\n", edge->name));
      if (edge->area_left == AREA_OUT)
        reverse_edge (edge);
      start = edge->start;
      gsk_path_builder_move_to (builder, start->p.x, start->p.y);
      gsk_curve_builder_to (&edge->curve, builder);
      edge->collected = TRUE;

      /* collect segments, following through nodes */
      for (edge = find_next (edge); edge; edge = find_next (edge))
        {
          g_assert (!edge->interior);

          if (edge->collected)
            {
              g_warning ("find_next returned a collected edge, falling off\n");
              break;
            }

          GSK_NOTE (PATHS, g_print ("append %s\n", edge->name));
          gsk_curve_builder_to (&edge->curve, builder);
          edge->collected = TRUE;

          if (edge->curve.op == GSK_PATH_CLOSE)
            {
              GSK_NOTE (PATHS, g_print ("explicitly closed\n"));
              break;
            }

          if (edge->end == start)
            {
              GSK_NOTE (PATHS, g_print ("implicitly closed\n"));
              gsk_path_builder_close (builder);
              break;
            }
        }
    }

  gsk_path_measure_unref (opdata.first);
  if (opdata.second)
    gsk_path_measure_unref (opdata.second);

  g_list_free_full (opdata.edges, (GDestroyNotify) edge_free);
  g_list_free_full (opdata.nodes, (GDestroyNotify) node_free);

  return gsk_path_remove_unclosed (gsk_path_builder_free_to_path (builder));
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
