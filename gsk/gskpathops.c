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

#include <math.h>

#include "gskpathprivate.h"

#include "gskcontourprivate.h"
#include "gskcurveprivate.h"
#include "gskpathbuilder.h"

#include "gdk/gdkprivate.h"

#define PATH_OPS_DEBUG

#if defined(PATH_OPS_DEBUG) && !defined(G_ENABLE_DEBUG)
#define G_ENABLE_DEBUG
#endif

#ifdef G_ENABLE_DEBUG
#define DEBUG(opdata, ...) \
  if ((opdata)->debug) \
    { \
      g_print (__VA_ARGS__); \
      g_print ("\n"); \
    }
#else
#define DEBUG(opdata, ...)
#endif

/* {{{ General utilities */

#define RAD_TO_DEG(r) ((r + M_PI)*180.0/M_PI)
#define DEG_TO_RAD(d) ((d)*M_PI/180.0)

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

/* This assumes the endpoints are the same */
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
  gboolean interior; // area_left == area_right
  gboolean coincides; // part of both paths
  gboolean collected;
  gboolean remove;
  float start_angle;
  float end_angle;
  int path_num;
  int curve_num;
  int intersect_next;
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

  for (int i = c1->edges->len - 1; i >= 0; i--)
    {
      Edge *edge = g_ptr_array_index (c1->edges, i);

      if (edge->start == edge->end)
        edge->remove = TRUE;
    }
}

/* }}} */
/* {{{ Path Op Data */

typedef struct
{
  GskPathOp operation;
  GskFillRule fill_rule;
  GskPath *first;
  GskPath *second;

  GList *edges;
  GList *nodes;

  Edge *start;
  int curve_num;
  int path_num;

#ifdef G_ENABLE_DEBUG
  gboolean debug;
#endif
} PathOpData;

static void
pathopdata_init (PathOpData  *opdata,
                 GskPathOp    operation,
                 GskFillRule  fill_rule,
                 GskPath     *first,
                 GskPath     *second)
{
  memset (opdata, 0, sizeof (PathOpData));

  opdata->operation = operation;
  opdata->fill_rule = fill_rule;
  opdata->first = gsk_path_ref (first);
  if (second)
    opdata->second = gsk_path_ref (second);

#ifdef G_ENABLE_DEBUG
  opdata->debug = g_strcmp0 (g_getenv ("GSK_PATH_DEBUG"), "1") == 0;
#endif
}

static void
pathopdata_clear (PathOpData *opdata)
{
  gsk_path_unref (opdata->first);
  if (opdata->second)
    gsk_path_unref (opdata->second);

  g_list_free_full (opdata->edges, (GDestroyNotify) edge_free);
  g_list_free_full (opdata->nodes, (GDestroyNotify) node_free);
}

/* }}} */
/* {{{ Debugging */

#ifdef G_ENABLE_DEBUG
static void
dump_node (Node *c)
{
  g_print ("%s%s %f %f\n", c->name, c->inconsistent ? " BAD" : "", c->p.x, c->p.y);

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

/* Check that our graph structure is intact:
 * Every edge is included in the edges of its
 * start and end nodes.
 */
static void
validate_edges (PathOpData *opdata)
{
  for (GList *l = opdata->edges; l; l = l->next)
    {
      Edge *c = l->data;

      if (opdata->debug)
        {
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
          char *s = gsk_curve_to_string (&c->curve);
          g_print ("%s %s\n", s, ind2);
          g_free (s);
        }

      g_assert (c->curve.op == GSK_PATH_LINE ||
                c->curve.op == GSK_PATH_QUAD ||
                c->curve.op == GSK_PATH_CUBIC);
      g_assert (g_ptr_array_find (c->start->edges, c, NULL));
      g_assert (g_ptr_array_find (c->end->edges, c, NULL));
    }
}

/* Check that our graph structure is intact:
 * All edges of a node are connected to that node.
 */
static void
validate_nodes (PathOpData *opdata)
{
  for (GList *l = opdata->nodes; l; l = l->next)
    {
      Node *c = l->data;

      for (int i = 0; i < c->edges->len; i++)
        {
          Edge *edge G_GNUC_UNUSED;

          edge = g_ptr_array_index (c->edges, i);

          g_assert (edge->start == c || edge->end == c);
        }
    }
}
#endif /* G_ENABLE_DEBUG */

#ifdef PATH_OPS_DEBUG

/* Use with dot -Tpng -O inconsistent.dot to get some visualization
 * of the graph we ended up with. Inconsistent nodes appear in red.
 */
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

      g_string_append_printf (s, "\"%p\" [label=\"%s\",color=%s]\n", c, c->name, c->inconsistent ? (c->inconsistent == 1 ? "red" : "purple") : (c->boundaries == 0 ? "gray" : "black"));
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
  gsk_curve_init_foreach (&edge->curve, op, pts, n_pts, weight);

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
      if (opdata->debug)
        node->name = g_strdup_printf ("start %d", opdata->curve_num);
#endif

      opdata->nodes = g_list_prepend (opdata->nodes, node);

      edge->start = node;
    }

  node = g_new0 (Node, 1);
  node->p = *gsk_curve_get_end_point (&edge->curve);
  node->edges = g_ptr_array_new ();
#ifdef G_ENABLE_DEBUG
  if (opdata->debug)
    node->name = g_strdup_printf ("end %d", opdata->curve_num);
#endif

  opdata->nodes = g_list_prepend (opdata->nodes, node);

  edge->end = node;

  g_ptr_array_add (edge->start->edges, edge);
  g_ptr_array_add (edge->end->edges, edge);

  opdata->edges = g_list_prepend (opdata->edges, edge);

#ifdef G_ENABLE_DEBUG
  if (opdata->debug)
    {
      const char *opname[] = { "Move", "Close", "Line", "Quad", "Cubic", "Conic" };
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
  GskPath *p;

  p = gsk_path_remove_unclosed (gsk_path_ref (path));
  gsk_path_foreach (p, -1, collect_cb, opdata);
  gsk_path_unref (p);

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

static void
split_edges (PathOpData *opdata)
{
  GList *l, *ll;

  l = opdata->edges;
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
          GskPathIntersection kind[9];
          SplitPoint sp[9];
          int n;
          GskCurve cs = { 0, };
          GskCurve ce = { 0, };
          Edge *cd;
          GList *before;
#ifdef G_ENABLE_DEBUG
          char *name = NULL;
#endif
          int path_num1, path_num2;
          int curve_num1, curve_num2;

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

          n = gsk_curve_intersect (&cd1->curve, &cd2->curve, t1, t2, p, kind, G_N_ELEMENTS (t1));
          /* FIXME make use of kind */

#ifdef G_ENABLE_DEBUG
          if (n > 0)
            DEBUG (opdata, "%d intersections between %s and %s", n, cd1->name, cd2->name);
#endif

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
            sp[i] = (SplitPoint) { t1[i], t2[i], p[i], NULL };

          qsort (sp, n, sizeof (SplitPoint), compare_t1);

#ifdef G_ENABLE_DEBUG
          if (opdata->debug)
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

                  opdata->nodes = g_list_prepend (opdata->nodes, sp[i].node);

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
                  if (opdata->debug)
                    {
                      sp[i].node->name = g_strdup_printf ("split %s/%s", name, cd2->name);
                      if (i == 0)
                        {
                          char *nn = g_strdup_printf ("%s.0", name);
                          g_free (cd1->name);
                          cd1->name = nn;
                        }
                      cd->name = g_strdup_printf ("%s.%d", name, i + 1);

                      if (i == 0)
                        {
                          char *s = gsk_curve_to_string (&cd1->curve);
                          g_print ("split %s from %s at %g: %s\n", cd1->name, name, sp[i].t1, s);
                          g_free (s);
                        }
                      char * s = gsk_curve_to_string (&cd->curve);
                      g_print ("split %s from %s at %g: %s", cd->name, name, sp[i].t1, s);
                      g_free (s);
                    }
#endif

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
          if (opdata->debug)
            g_free (name);
#endif

          qsort (sp, n, sizeof (SplitPoint), compare_t2);

#ifdef G_ENABLE_DEBUG
          if (opdata->debug)
            name = g_strdup (cd2->name);
#endif
          for (int i = 0; i < n; i++)
            {
              if (NEAR (sp[i].t2, 0))
                {
                  for (int k = 0; k < n; k++)
                    {
                      if (sp[k].node == cd2->start)
                        sp[k].node = sp[i].node;
                    }
                  merge_nodes (&opdata->nodes, sp[i].node, cd2->start);
                }
              else if (NEAR (sp[i].t2, 1))
                {
                  for (int k = 0; k < n; k++)
                    {
                      if (sp[k].node == cd2->end)
                        sp[k].node = sp[i].node;
                    }
                  merge_nodes (&opdata->nodes, sp[i].node, cd2->end);
                }
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
                  if (opdata->debug)
                    {
                      if (i == 0)
                        {
                          char *nn = g_strdup_printf ("%s.0", name);
                          g_free (cd2->name);
                          cd2->name = nn;
                        }
                      cd->name = g_strdup_printf ("%s.%d", name, i + 1);

                      if (i == 0)
                        {
                          char * s = gsk_curve_to_string (&cd2->curve);
                          g_print ("split %s from %s at %g: %s\n", cd2->name, name, sp[i].t2, s);
                          g_free (s);
                        }
                      char *s = gsk_curve_to_string (&cd->curve);
                      g_print ("split %s from %s at %g: %s\n", cd->name, name, sp[i].t2, s);
                      g_free (s);
                    }
#endif

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
          if (opdata->debug)
            g_free (name);
#endif

          ll = ll->next;
        }

      l = l->next;
    }
}

/* }}} */
/* {{{ Classification helpers */

/* Each edge that comes into a node is either interior, or it is
 * a boundary between inner and outer. There must be an even number
 * of non-interior edges, and the area must alternate between inner
 * and outer.
 */

static void
check_minimal_consistency (Node *c)
{
  c->inconsistent = 0;
  c->boundaries = 0;

  for (int i = 0; i < c->edges->len; i++)
    {
      Edge *edge = g_ptr_array_index (c->edges, i);

      if (!edge->interior)
        c->boundaries++;
    }

  if (c->boundaries % 2 != 0)
    {
      /* Edges are misclassified */
      c->inconsistent = 1;
    }
}

static void
check_consistency (Node *c)
{
  check_minimal_consistency (c);

  if (c->inconsistent)
    return;

  for (int i = 0; i < c->edges->len; i++)
    {
      Edge *edge = g_ptr_array_index (c->edges, i);
      Edge *edge2 = g_ptr_array_index (c->edges, (i + 1) % c->edges->len);
      AreaClassification area1, area2, area;

      if (edge->end == c)
        {
          area1 = edge->area_right1;
          area2 = edge->area_right2;
          area = edge->area_right;
        }
      else
        {
          area1 = edge->area_left1;
          area2 = edge->area_left2;
          area = edge->area_left;
        }

      if (edge2->end == c)
        {
          if (area1 != edge2->area_left1 ||
              area2 != edge2->area_left2 ||
              area != edge2->area_left)
            c->inconsistent = 2;
        }
      else
        {
          if (area1 != edge2->area_right1 ||
              area2 != edge2->area_right2 ||
              area != edge2->area_right)
            c->inconsistent = 2;
        }

      if (!edge->coincides)
        {
          if ((edge->path_num == 0 &&
               edge->area_left2 != edge->area_right2) ||
              (edge->path_num == 1 &&
               edge->area_left1 != edge->area_right1))
            c->inconsistent = 2;
        }

      if (c->inconsistent)
        return;
    }
}

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
    case GSK_PATH_OP_UNION:        return (c1 == AREA_IN) || (c2 == AREA_IN)  ? AREA_IN : AREA_OUT;
    case GSK_PATH_OP_INTERSECTION: return (c1 == AREA_IN) && (c2 == AREA_IN)  ? AREA_IN : AREA_OUT;
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
  graphene_vec2_t tangent;

  if (edge->area_left != AREA_UNKNOWN && edge->area_right != AREA_UNKNOWN)
    return;

  gsk_curve_get_point (&edge->curve, 0.5, &pos);
  gsk_curve_get_tangent (&edge->curve, 0.5, &tangent);

  pos1.x = pos.x + 0.5 * graphene_vec2_get_y (&tangent);
  pos1.y = pos.y - 0.5 * graphene_vec2_get_x (&tangent);
  pos2.x = pos.x - 0.5 * graphene_vec2_get_y (&tangent);
  pos2.y = pos.y + 0.5 * graphene_vec2_get_x (&tangent);

  if (edge->path_num == 1 && !edge->coincides)
    {
      /* Classifying wrt to the other path; check the point
       * on the curve, which is safe since we're already
       * intersected. The only case we need to avoid is if
       * this edge coincides with an edge on the other path.
       */
      edge->area_left1 = edge->area_right1 =
        gsk_path_in_fill (opdata->first, &pos, opdata->fill_rule) ? AREA_IN : AREA_OUT;
    }
  else
    {
      if (edge->area_left1 == AREA_UNKNOWN)
        edge->area_left1 = gsk_path_in_fill (opdata->first, &pos1, opdata->fill_rule) ? AREA_IN : AREA_OUT;
      if (edge->area_right1 == AREA_UNKNOWN)
        edge->area_right1 = gsk_path_in_fill (opdata->first, &pos2, opdata->fill_rule) ? AREA_IN : AREA_OUT;
    }

  if (opdata->second)
    {
      if (edge->path_num == 0 && !edge->coincides)
        {
          /* Classifying wrt to the other path, see above */
          edge->area_left2 = edge->area_right2 =
            gsk_path_in_fill (opdata->second, &pos, opdata->fill_rule) ? AREA_IN : AREA_OUT;
        }
      else
        {
          if (edge->area_left2 == AREA_UNKNOWN)
            edge->area_left2 = gsk_path_in_fill (opdata->second, &pos1, opdata->fill_rule) ? AREA_IN : AREA_OUT;
          if (edge->area_right2 == AREA_UNKNOWN)
            edge->area_right2 = gsk_path_in_fill (opdata->second, &pos2, opdata->fill_rule) ? AREA_IN : AREA_OUT;
        }
    }
  else
    {
      edge->area_left2 = AREA_OUT;
      edge->area_right2 = AREA_OUT;
    }

  edge->area_left = apply_op (opdata->operation, edge->area_left1, edge->area_left2);
  edge->area_right = apply_op (opdata->operation, edge->area_right1, edge->area_right2);

  edge->interior = edge->area_left == edge->area_right;
}

static inline void
reset_classification (Edge *edge)
{
  edge->area_left = AREA_UNKNOWN;
  edge->area_right = AREA_UNKNOWN;
  edge->area_left1 = AREA_UNKNOWN;
  edge->area_right1 = AREA_UNKNOWN;
  edge->area_left2 = AREA_UNKNOWN;
  edge->area_right2 = AREA_UNKNOWN;
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

static inline float
get_turning_direction (Edge *c1,
                       Node *node)
{
  graphene_point_t p;

  if (c1->curve.op == GSK_PATH_CUBIC)
    gsk_curve_get_point (&c1->curve, c1->start == node ? 0.333 : 0.666, &p);
  else
    gsk_curve_get_point (&c1->curve, 0.5, &p);

  return atan2 (- (p.y - node->p.y), p.x - node->p.x);
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
      /* Deal with curves that come in at the same angle
       * by looking in which direction they turn
       */
      f1 = get_turning_direction (c1, c);
      f2 = get_turning_direction (c2, c);
    }

  return f1 < f2 ? -1 : (f1 > f2 ? 1 : 0);
}

static void
classify_edges (PathOpData *opdata)
{
  GList *l;

  for (l = opdata->edges; l; l = l->next)
    {
      Edge *edge = l->data;

      if (edge->remove)
        continue;

      compute_coincidence (edge);
      compute_angles (edge);
    }

  l = opdata->edges;
  while (l)
    {
      Edge *edge = l->data;
      GList *next = l->next;

      if (edge->remove)
        {
          g_ptr_array_remove (edge->start->edges, edge);
          g_ptr_array_remove (edge->end->edges, edge);
          opdata->edges = g_list_remove (opdata->edges, edge);
          edge_free (edge);
        }

      l = next;
    }

  for (l = opdata->nodes; l; l = l->next)
    {
      Node *c = l->data;

      g_ptr_array_sort_with_data (c->edges, compare_angle, c);
    }

  /* We do classification after sorting, so we can possibly
   * use node order during propagation.
   */
  for (l = opdata->edges; l; l = l->next)
    {
      Edge *edge = l->data;

      classify_boundary (edge, opdata);
      propagate_classification (edge);
    }

  for (l = opdata->nodes; l; l = l->next)
    {
      Node *c = l->data;

      check_consistency (c);
    }
}

/* }}} */
/* {{{ Consistency helpers */

static void
propagate_changed_classification (Edge     *edge,
                                  gboolean  forward)
{
  Node *c = forward ? edge->end : edge->start;
  Edge *other_edge;
  guint idx;

  if (c->edges->len != 2)
    {
      check_minimal_consistency (c);
      return;
    }

  g_ptr_array_find (c->edges, edge, &idx);
  other_edge = g_ptr_array_index (c->edges, 1 - idx);

  if (other_edge->area_left != edge->area_left ||
      other_edge->area_right != edge->area_right ||
      other_edge->interior != edge->interior)
    {
      copy_classification (edge, other_edge);
      propagate_changed_classification (other_edge, forward);
    }
}

static void
propagate_changes (Node *c)
{
  for (unsigned int i = 0; i < c->edges->len; i++)
    {
      Edge *edge = g_ptr_array_index (c->edges, i);

      propagate_changed_classification (edge, edge->start == c);
    }
}

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

static Edge *
other_edge (Node *n, Edge *e)
{
  if (n->edges->len == 2)
    {
      if (e == g_ptr_array_index (n->edges, 0))
        return g_ptr_array_index (n->edges, 1);
      else
        return g_ptr_array_index (n->edges, 0);
    }

  return NULL;
}

/* Return the intersection (i.e. >2-edge) node
 * that you arrive at by following the path
 * starting with edge e.
 */
static Node *
other_end (Node *n, Edge *e)
{
  gboolean forward = e->start == n;

  do {
    Node *n2 = forward ? e->end : e->start;
    e = other_edge (n2, e);
    if (e == NULL)
      return n2;
    if (n2 == n)
      return NULL;
  } while (1);
}

/* Check if all the edges along the path
 * from n via e agree on classification.
 */
static gboolean
path_is_consistent (Node *n, Edge *e, PathOpData *opdata)
{
  AreaClassification area_left = e->area_left;
  AreaClassification area_right = e->area_right;
  gboolean interior = e->interior;
  gboolean forward = e->start == n;

  do {
    Node *n2 = forward ? e->end : e->start;
    e = other_edge (n2, e);
    if (e == NULL)
      return TRUE;
    if (n2 == n)
      return TRUE;

    /* FIXME: do this without changing e */
    reset_classification (e);
    classify_boundary (e, opdata);

    if (e->area_left != area_left || e->area_right != area_right || e->interior != interior)
      return FALSE;
  } while (1);
}

/* Apply heuristic fixes to make the graph more consistent.
 *
 * First idea: If we find a path connecting two odd parity nodes,
 * and we toggle the path from interior to boundary (or the other
 * way around), then both nodes now have even parity. A good
 * candidate for such a path is one where some of the edges
 * disagree on classification (when explicitly computing it,
 * that is - they will be set the the same classification via
 * propagation at the outset).
 *
 * Second idea: Recomputing the classification of every edge
 * around a node might yield different results (since some of
 * the current edge classification are probably propagated).
 */
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

#ifdef G_ENABLE_DEBUG
  if (opdata->debug)
    {
      g_print ("found %d bad nodes\n", g_list_length (bad));

      for (GList *l = bad; l; l = l->next)
        {
          Node *c = l->data;
          dump_node (c);
        }
    }
#endif

  while (bad != NULL)
    {
      Node *n1 = bad->data;
      Node *n2 = NULL;
      Edge *edge = NULL;

      bad = g_list_remove (bad, n1);

      check_consistency (n1);
      if (!n1->inconsistent)
        {
          DEBUG (opdata, "inconsistency disappeared");
          continue;
        }

      if (n1->inconsistent == 1)
        {
          for (GList *l = bad; l; l = l->next)
            {
              Edge *fallback_edge = NULL;

              n2 = NULL;

              for (unsigned int i = 0; i < n1->edges->len; i++)
                {
                  Edge *e = g_ptr_array_index (n1->edges, i);
                  Node *other = other_end (n1, e);
                  if (other == n1)
                    {
                      /* Ignore loops, since they don't help us for parity */
                      continue;
                    }

                  if (other && other->inconsistent == 1)
                    {
                      if ((e->start == n1 && e->end == other) ||
                          (e->start == other && e->end == n1))
                        {
                          /* Just one hop, keep this one for fallback */
                          fallback_edge = e;
                          continue;
                        }

                      if (path_is_consistent (n1, e, opdata))
                        {
                          /* Not a good candidate */
                          continue;
                        }

                      /* We found a connection between two bad nodes
                       * where some of the edges along the way disagree on
                       * classification - a good candidate for fixups.
                       */
                      bad = g_list_remove (bad, other);
                      n2 = other;
                      edge = e;
                      break;
                    }
                }

              if (n2 == NULL && fallback_edge != NULL)
                {
                  edge = fallback_edge;
                  if (edge->start == n1)
                    n2 = edge->end;
                  else
                    n2 = edge->start;
                  bad = g_list_remove (bad, n2);
                }

              if (n2 != NULL)
                break;
            }

          if (n2 != NULL)
            {
              /* If we have two neighboring nodes with an odd
               * boundary count, we can try to fix it by toggling
               * one of the connecting edges.
               */

              if (n1->inconsistent == 1)
                {
                  DEBUG (opdata, "found 2 odd boundary nodes");
                  DEBUG (opdata, "toggling %s from %s to %s",
                         edge->name,
                         edge->interior ? "interior" : "boundary",
                         edge->interior ? "boundary" : "interior");
                  edge->interior = !edge->interior;
                  propagate_changed_classification (edge, edge->start == n1);
                  check_minimal_consistency (n1);
                }
            }
        }

      if (n1->inconsistent == 1)
        {
          /* Try to reclassify.
           * This may help if some of the classifications
           * were propagated.
           *
           * FIXME: we should only keep these changes if
           * the node is consistent afterwards.
           */
          for (int i = 0; i < n1->edges->len; i++)
            {
              edge = g_ptr_array_index (n1->edges, i);

              reset_classification (edge);
              classify_boundary (edge, opdata);
            }

          propagate_changes (n1);
          check_minimal_consistency (n1);
        }
    }
}

/* }}} */
/* {{{ Reassembly helpers */

static Edge *
find_next (PathOpData *opdata,
           Edge       *edge)
{
  Node *c = edge->end;
  Edge *next = NULL;
  Edge *next_fallback = NULL;
  guint idx;
  guint dir;

  g_assert (c != NULL);
  g_assert (c->edges != NULL);

#ifdef G_ENABLE_DEBUG
  if (opdata->debug)
    {
      g_print ("%s ends at: \n", edge->name);
      dump_node (edge->end);
    }
#endif

  /* Edges are sorted counterclockwise by their tangent.
   * We pick the next eligible edge to the left
   * or to the right of curve, depending on whether
   * the left or right is inside.
   */
  g_ptr_array_find (c->edges, edge, &idx);
  dir = edge->area_left == AREA_IN ? c->edges->len - 1 : 1;

  DEBUG (opdata, "picking %s", edge->area_left == AREA_IN ? "cw" : "ccw");

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

static void
reassemble_contours (PathOpData     *opdata,
                     GskPathBuilder *builder)
{
  GList *l;

  for (l = opdata->edges; l; l = l->next)
    {
      Edge *edge = l->data;
      Node *start;

      if (edge->collected || edge->interior)
        continue;

      DEBUG (opdata, "start new contour %s", edge->name);

      if (edge->area_left == AREA_OUT)
        reverse_edge (edge);
      start = edge->start;
      gsk_path_builder_move_to (builder, start->p.x, start->p.y);
      gsk_curve_builder_to (&edge->curve, builder);
      edge->collected = TRUE;

      /* collect segments, following through nodes */
      for (edge = find_next (opdata, edge); edge; edge = find_next (opdata, edge))
        {
          g_assert (!edge->interior);

          if (edge->collected)
            {
              g_warning ("find_next returned a collected edge, falling off");
              break;
            }

          DEBUG (opdata, "append %s", edge->name);

          gsk_curve_builder_to (&edge->curve, builder);
          edge->collected = TRUE;

          if (edge->curve.op == GSK_PATH_CLOSE)
            {
              DEBUG (opdata, "explicitly closed");
              break;
            }

          if (edge->end == start)
            {
              DEBUG (opdata, "implicitly closed");
              gsk_path_builder_close (builder);
              break;
            }
        }
    }
}

/* }}} */
/* {{{ Pathops implementation */

/*
 * The general plan of operation is as follows:
 *
 * 1. Collect all the edges in a list.
 *
 * 2. Add all intersections, splitting the edges as needed,
 *    and keep Node structs that record which edges are meeting
 *    at which intersections. Remove coinciding edges.
 *
 * 3. Sort the edges at each node, counterclockwise.
 *
 * 4. Classify each edge as boundary or not. This is where
 *    the different boolean ops differ from each other.
 *
 * 5. Fix up the resulting graph.
 *
 * 6. Walk the graph, taking the proper turns at each node, to
 *    reassemble contours. Continue doing so until all boundary
 *    edges have been added to a contour.
 *
 * We try to identify places where our graph is inconsistent by
 * checking some invariants:
 *
 * a) At every node, an even number of boundary edges must meet
 *
 * b) Neighboring edges of a node must agree on the area between them
 *
 * We apply some heuristic fixes to patch up these inconsistencies.
 */
GskPath *
gsk_path_op (GskPathOp    operation,
             GskFillRule  fill_rule,
             GskPath     *first,
             GskPath     *second)
{
  PathOpData opdata;
  GskPathBuilder *builder;

  pathopdata_init (&opdata, operation, fill_rule, first, second);

  builder = gsk_path_builder_new ();

  DEBUG (&opdata, "collecting");
  collect_edges (first, &opdata);
  if (second)
    collect_edges (second, &opdata);

  opdata.edges = g_list_reverse (opdata.edges);
  opdata.nodes = g_list_reverse (opdata.nodes);

#ifdef G_ENABLE_DEBUG
  validate_edges (&opdata);
  validate_nodes (&opdata);
#endif

  DEBUG (&opdata, "splitting");
  split_edges (&opdata);

#ifdef G_ENABLE_DEBUG
  validate_edges (&opdata);
  validate_nodes (&opdata);
#endif

  DEBUG (&opdata, "classifying");
  classify_edges (&opdata);

#ifdef G_ENABLE_DEBUG
  validate_edges (&opdata);
  validate_nodes (&opdata);
#endif

  if (graph_has_inconsistencies (&opdata))
    {
#ifdef PATH_OPS_DEBUG
      dump_dotfile (opdata.edges, opdata.nodes, "inconsistent.dot");

      if (opdata.debug)
        {
          for (GList *l = opdata.nodes; l; l = l->next)
            {
              Node *n = l->data;
              if (n->inconsistent == 1)
                gsk_path_builder_add_circle (builder, &n->p, 4);
              else if (n->inconsistent == 2)
                gsk_path_builder_add_circle (builder, &n->p, 2);
            }
         }
#endif

      DEBUG (&opdata, "fixups");
      apply_fixups (&opdata);

#ifdef PATH_OPS_DEBUG
      dump_dotfile (opdata.edges, opdata.nodes, "after-fixups.dot");
#endif
    }

  DEBUG (&opdata, "reassembling");
  reassemble_contours (&opdata, builder);

  /* Cleanup */
  pathopdata_clear (&opdata);

  return gsk_path_remove_unclosed (gsk_path_builder_free_to_path (builder));
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
