/*
 * Copyright © 2020 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gskpathprivate.h"

#include "gskcurveprivate.h"
#include "gskpathbuilder.h"


typedef struct _GskContour GskContour;
typedef struct _GskContourClass GskContourClass;

struct _GskPath
{
  /*< private >*/
  guint ref_count;

  GskPathFlags flags;

  gsize n_contours;
  GskContour *contours[];
  /* followed by the contours data */
};

/**
 * GskPath:
 *
 * `GskPath` is used to describe lines and curves that are more
 * complex than simple rectangles.
 *
 * A `GskPath` struct is a reference counted struct
 * and should be treated as opaque.
 *
 * `GskPath` is an immutable struct. After creation, you cannot change
 * the types it represents. Instead, new `GskPath` have to be created.
 * The `GskPathBuilder` structure is meant to help in this endeavor.
 */

G_DEFINE_BOXED_TYPE (GskPath, gsk_path,
                     gsk_path_ref,
                     gsk_path_unref)


GskPath *
gsk_path_new_from_contours (const GSList *contours)
{
  GskPath *path;
  const GSList *l;
  gsize size;
  gsize n_contours;
  guint8 *contour_data;
  GskPathFlags flags;

  flags = GSK_PATH_CLOSED | GSK_PATH_FLAT;
  size = 0;
  n_contours = 0;
  for (l = contours; l; l = l->next)
    {
      GskContour *contour = l->data;

      n_contours++;
      size += sizeof (GskContour *);
      size += gsk_contour_get_size (contour);
      flags &= gsk_contour_get_flags (contour);
    }

  path = g_malloc0 (sizeof (GskPath) + size);
  path->ref_count = 1;
  path->flags = flags;
  path->n_contours = n_contours;
  contour_data = (guint8 *) &path->contours[n_contours];
  n_contours = 0;

  for (l = contours; l; l = l->next)
    {
      GskContour *contour = l->data;

      path->contours[n_contours] = (GskContour *) contour_data;
      gsk_contour_copy ((GskContour *) contour_data, contour);
      size = gsk_contour_get_size (contour);
      contour_data += size;
      n_contours++;
    }

  return path;
}

/**
 * gsk_path_new_from_cairo:
 * @path: a Cairo path
 *
 * This is a convenience function that constructs a `GskPath` from a Cairo path.
 *
 * You can use cairo_copy_path() to access the path from a Cairo context.
 *
 * Returns: a new `GskPath`
 **/
GskPath *
gsk_path_new_from_cairo (const cairo_path_t *path)
{
  GskPathBuilder *builder;
  gsize i;

  g_return_val_if_fail (path != NULL, NULL);

  builder = gsk_path_builder_new ();

  for (i = 0; i < path->num_data; i += path->data[i].header.length)
    {
      const cairo_path_data_t *data = &path->data[i];

      switch (data->header.type)
      {
        case CAIRO_PATH_MOVE_TO:
          gsk_path_builder_move_to (builder, data[1].point.x, data[1].point.y);
          break;

        case CAIRO_PATH_LINE_TO:
          gsk_path_builder_line_to (builder, data[1].point.x, data[1].point.y);
          break;

        case CAIRO_PATH_CURVE_TO:
          gsk_path_builder_curve_to (builder,
                                     data[1].point.x, data[1].point.y,
                                     data[2].point.x, data[2].point.y,
                                     data[3].point.x, data[3].point.y);
          break;

        case CAIRO_PATH_CLOSE_PATH:
          gsk_path_builder_close (builder);
          break;

        default:
          g_assert_not_reached ();
          break;
      }
    }

  return gsk_path_builder_free_to_path (builder);
}

/**
 * gsk_path_ref:
 * @self: a `GskPath`
 *
 * Increases the reference count of a `GskPath` by one.
 *
 * Returns: the passed in `GskPath`.
 **/
GskPath *
gsk_path_ref (GskPath *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  self->ref_count++;

  return self;
}

/**
 * gsk_path_unref:
 * @self: a `GskPath`
 *
 * Decreases the reference count of a `GskPath` by one.
 *
 * If the resulting reference count is zero, frees the path.
 **/
void
gsk_path_unref (GskPath *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  self->ref_count--;
  if (self->ref_count > 0)
    return;

  g_free (self);
}

const GskContour *
gsk_path_get_contour (GskPath *path,
                      gsize    i)
{
  return path->contours[i];
}

/**
 * gsk_path_print:
 * @self: a `GskPath`
 * @string:  The string to print into
 *
 * Converts @self into a human-readable string representation suitable
 * for printing.
 *
 * The string is compatible with
 * [SVG path syntax](https://www.w3.org/TR/SVG11/paths.html#PathData),
 * with the exception that conic curves will generate a string of the
 * form "O x1 y1, x2 y2, w" where x1, y1 is the control point, x2, y2
 * is the end point, and w is the weight.
 **/
void
gsk_path_print (GskPath *self,
                GString *string)
{
  gsize i;

  g_return_if_fail (self != NULL);
  g_return_if_fail (string != NULL);

  for (i = 0; i < self->n_contours; i++)
    {
      if (i > 0)
        g_string_append_c (string, ' ');
      gsk_contour_print (self->contours[i], string);
    }
}

/**
 * gsk_path_to_string:
 * @self: a `GskPath`
 *
 * Converts the path into a string that is suitable for printing.
 *
 * You can use this function in a debugger to get a quick overview
 * of the path.
 *
 * This is a wrapper around [method@Gsk.Path.print], see that function
 * for details.
 *
 * Returns: A new string for @self
 **/
char *
gsk_path_to_string (GskPath *self)
{
  GString *string;

  g_return_val_if_fail (self != NULL, NULL);

  string = g_string_new ("");

  gsk_path_print (self, string);

  return g_string_free (string, FALSE);
}

static gboolean
gsk_path_to_cairo_add_op (GskPathOperation        op,
                          const graphene_point_t *pts,
                          gsize                   n_pts,
                          float                   weight,
                          gpointer                cr)
{
  switch (op)
  {
    case GSK_PATH_MOVE:
      cairo_move_to (cr, pts[0].x, pts[0].y);
      break;

    case GSK_PATH_CLOSE:
      cairo_close_path (cr);
      break;

    case GSK_PATH_LINE:
      cairo_line_to (cr, pts[1].x, pts[1].y);
      break;

    case GSK_PATH_CURVE:
      cairo_curve_to (cr, pts[1].x, pts[1].y, pts[2].x, pts[2].y, pts[3].x, pts[3].y);
      break;

    case GSK_PATH_CONIC:
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  return TRUE;
}

/**
 * gsk_path_to_cairo:
 * @self: a `GskPath`
 * @cr: a cairo context
 *
 * Appends the given @path to the given cairo context for drawing
 * with Cairo.
 *
 * This may cause some suboptimal conversions to be performed as Cairo
 * may not support all features of `GskPath`.
 *
 * This function does not clear the existing Cairo path. Call
 * cairo_new_path() if you want this.
 **/
void
gsk_path_to_cairo (GskPath *self,
                   cairo_t *cr)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (cr != NULL);

  gsk_path_foreach_with_tolerance (self,
                                   GSK_PATH_FOREACH_ALLOW_CURVE,
                                   cairo_get_tolerance (cr),
                                   gsk_path_to_cairo_add_op,
                                   cr);
}

/*
 * gsk_path_get_n_contours:
 * @path: a #GskPath
 *
 * Gets the number of contours @path is composed out of.
 *
 * Returns: the number of contours in @path
 **/
gsize
gsk_path_get_n_contours (GskPath *path)
{
  return path->n_contours;
}

/**
 * gsk_path_is_empty:
 * @self: a `GskPath`
 *
 * Checks if the path is empty, i.e. contains no lines or curves.
 *
 * Returns: %TRUE if the path is empty
 **/
gboolean
gsk_path_is_empty (GskPath *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->n_contours == 0;
}

/**
 * gsk_path_get_bounds:
 * @self: a `GskPath`
 * @bounds: (out caller-allocates): the bounds of the given path
 *
 * Computes the bounds of the given path.
 *
 * The returned bounds may be larger than necessary, because this
 * function aims to be fast, not accurate. The bounds are guaranteed
 * to contain the path.
 *
 * It is possible that the returned rectangle has 0 width and/or height.
 * This can happen when the path only describes a point or an
 * axis-aligned line.
 *
 * If the path is empty, %FALSE is returned and @bounds are set to
 * graphene_rect_zero(). This is different from the case where the path
 * is a single point at the origin, where the @bounds will also be set to
 * the zero rectangle but 0 will be returned.
 *
 * Returns: %TRUE if the path has bounds, %FALSE if the path is known
 *   to be empty and have no bounds.
 **/
gboolean
gsk_path_get_bounds (GskPath         *self,
                     graphene_rect_t *bounds)
{
  gsize i;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (bounds != NULL, FALSE);

  for (i = 0; i < self->n_contours; i++)
    {
      if (gsk_contour_get_bounds (self->contours[i], bounds))
        break;
    }

  if (i >= self->n_contours)
    {
      graphene_rect_init_from_rect (bounds, graphene_rect_zero ());
      return FALSE;
    }

  for (i++; i < self->n_contours; i++)
    {
      graphene_rect_t tmp;

      if (gsk_contour_get_bounds (self->contours[i], &tmp))
        graphene_rect_union (bounds, &tmp, bounds);
    }

  return TRUE;
}

/**
 * gsk_path_foreach:
 * @self: a `GskPath`
 * @flags: flags to pass to the foreach function. See `GskPathForeachFlags` for
 *   details about flags.
 * @func: (scope call) (closure user_data): the function to call for operations
 * @user_data: (nullable): user data passed to @func
 *
 * Calls @func for every operation of the path.
 *
 * Note that this only approximates @self, because paths can contain optimizations
 * for various specialized contours.
 *
 * Returns: %FALSE if @func returned %FALSE, %TRUE otherwise.
 **/
gboolean
gsk_path_foreach (GskPath             *self,
                  GskPathForeachFlags  flags,
                  GskPathForeachFunc   func,
                  gpointer             user_data)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (func, FALSE);

  return gsk_path_foreach_with_tolerance (self,
                                          flags,
                                          GSK_PATH_TOLERANCE_DEFAULT,
                                          func,
                                          user_data);
}

typedef struct _GskPathForeachTrampoline GskPathForeachTrampoline;
struct _GskPathForeachTrampoline
{
  GskPathForeachFlags flags;
  double tolerance;
  GskPathForeachFunc func;
  gpointer user_data;
};

static gboolean
gsk_path_foreach_trampoline_add_line (const graphene_point_t *from,
                                      const graphene_point_t *to,
                                      float                   from_progress,
                                      float                   to_progress,
                                      gpointer                data)
{
  GskPathForeachTrampoline *trampoline = data;

  return trampoline->func (GSK_PATH_LINE,
                           (graphene_point_t[2]) { *from, *to },
                           2,
                           0.0f,
                           trampoline->user_data);
}

static gboolean
gsk_path_foreach_trampoline_add_curve (const graphene_point_t points[4],
                                       gpointer               data)
{
  GskPathForeachTrampoline *trampoline = data;

  return trampoline->func (GSK_PATH_CURVE, points, 4, 0.0f, trampoline->user_data);
}

static gboolean
gsk_path_foreach_trampoline (GskPathOperation        op,
                             const graphene_point_t *pts,
                             gsize                   n_pts,
                             float                   weight,
                             gpointer                data)
{
  GskPathForeachTrampoline *trampoline = data;

  switch (op)
  {
    case GSK_PATH_MOVE:
    case GSK_PATH_CLOSE:
    case GSK_PATH_LINE:
      return trampoline->func (op, pts, n_pts, weight, trampoline->user_data);

    case GSK_PATH_CURVE:
      {
        GskCurve curve;

        if (trampoline->flags & GSK_PATH_FOREACH_ALLOW_CURVE)
          return trampoline->func (op, pts, n_pts, weight, trampoline->user_data);

        gsk_curve_init (&curve, gsk_pathop_encode (GSK_PATH_CURVE, pts));
        return gsk_curve_decompose (&curve,
                                    trampoline->tolerance,
                                    gsk_path_foreach_trampoline_add_line,
                                    trampoline);
      }

    case GSK_PATH_CONIC:
      {
         GskCurve curve;

        if (trampoline->flags & GSK_PATH_FOREACH_ALLOW_CONIC)
          return trampoline->func (op, pts, n_pts, weight, trampoline->user_data);
        else
          {
            gsk_curve_init (&curve, gsk_pathop_encode (GSK_PATH_CONIC, (graphene_point_t[4]) { pts[0], pts[1], { weight, }, pts[2] } ));

            if (trampoline->flags & GSK_PATH_FOREACH_ALLOW_CURVE)
              return gsk_curve_decompose_curve (&curve,
                                                trampoline->tolerance,
                                                gsk_path_foreach_trampoline_add_curve,
                                                trampoline);
            else
              return gsk_curve_decompose (&curve,
                                          trampoline->tolerance,
                                          gsk_path_foreach_trampoline_add_line,
                                          trampoline);
          }
      }


    default:
      g_assert_not_reached ();
      return FALSE;
  }
}

gboolean
gsk_path_foreach_with_tolerance (GskPath             *self,
                                 GskPathForeachFlags  flags,
                                 double               tolerance,
                                 GskPathForeachFunc   func,
                                 gpointer             user_data)
{
  GskPathForeachTrampoline trampoline;
  gsize i;

  /* If we need to massage the data, set up a trampoline here */
  if (flags != (GSK_PATH_FOREACH_ALLOW_CURVE | GSK_PATH_FOREACH_ALLOW_CONIC))
    {
      trampoline = (GskPathForeachTrampoline) { flags, tolerance, func, user_data };
      func = gsk_path_foreach_trampoline;
      user_data = &trampoline;
    }

  for (i = 0; i < self->n_contours; i++)
    {
      if (!gsk_contour_foreach (self->contours[i], tolerance, func, user_data))
        return FALSE;
    }

  return TRUE;
}

/* path parser and utilities */

static void
skip_whitespace (const char **p)
{
  while (g_ascii_isspace (**p))
    (*p)++;
}

static void
skip_optional_comma (const char **p)
{
  skip_whitespace (p);
  if (**p == ',')
    (*p)++;
}

static gboolean
parse_number (const char **p,
              double      *c)
{
  char *e;
  *c = g_ascii_strtod (*p, &e);
  if (e == *p)
    return FALSE;
  *p = e;
  skip_optional_comma (p);
  return TRUE;
}

static gboolean
parse_coordinate (const char **p,
                  double      *c)
{
  return parse_number (p, c);
}

static gboolean
parse_coordinate_pair (const char **p,
                       double      *x,
                       double      *y)
{
  double xx, yy;
  const char *o = *p;

  if (!parse_coordinate (p, &xx))
    {
      *p = o;
      return FALSE;
    }
  if (!parse_coordinate (p, &yy))
    {
      *p = o;
      return FALSE;
    }

  *x = xx;
  *y = yy;

  return TRUE;
}

static gboolean
parse_nonnegative_number (const char **p,
                          double      *x)
{
  const char *o = *p;
  double n;

  if (!parse_number (p, &n))
    return FALSE;

  if (n < 0)
    {
      *p = o;
      return FALSE;
    }

  *x = n;

  return TRUE;
}

static gboolean
parse_flag (const char **p,
            gboolean    *f)
{
  skip_whitespace (p);
  if (strchr ("01", **p))
    {
      *f = **p == '1';
      (*p)++;
      skip_optional_comma (p);
      return TRUE;
    }

  return FALSE;
}

static gboolean
parse_command (const char **p,
               char        *cmd)
{
  char *s;
  const char *allowed;

  if (*cmd == 'X')
    allowed = "mM";
  else
    allowed = "mMhHvVzZlLcCsStTqQoOaA";

  skip_whitespace (p);
  s = strchr (allowed, **p);
  if (s)
    {
      *cmd = *s;
      (*p)++;
      return TRUE;
    }
  return FALSE;
}

static gboolean
parse_string (const char **p,
              const char  *s)
{
  int len = strlen (s);
  if (strncmp (*p, s, len) != 0)
    return FALSE;
  (*p) += len;
  return TRUE;
}

static gboolean
parse_rectangle (const char **p,
                 double      *x,
                 double      *y,
                 double      *w,
                 double      *h)
{
  const char *o = *p;
  double w2;

  /* Check for M%g,%gh%gv%gh%gz without any intervening whitespace */
  if (parse_coordinate_pair (p, x, y) &&
      parse_string (p, "h") &&
      parse_coordinate (p, w) &&
      parse_string (p, "v") &&
      parse_coordinate (p, h) &&
      parse_string (p, "h") &&
      parse_coordinate (p, &w2) &&
      parse_string (p, "z") &&
      w2 == - *w)
    {
      skip_whitespace (p);

      return TRUE;
    }

  *p = o;
  return FALSE;
}

static gboolean
parse_circle (const char **p,
              double      *sx,
              double      *sy,
              double      *r)
{
  const char *o = *p;
  double r1, r2, r3, mx, my, ex, ey;

  /* Check for M%g,%gA%g,%g,0,1,0,%g,%gA%g,%g,0,1,0,%g,%g
   * without any intervening whitespace
   */
  if (parse_coordinate_pair (p, sx, sy) &&
      parse_string (p, "A") &&
      parse_coordinate_pair (p, r, &r1) &&
      parse_string (p, "0 0 0") &&
      parse_coordinate_pair (p, &mx, &my) &&
      parse_string (p, "A") &&
      parse_coordinate_pair (p, &r2, &r3) &&
      parse_string (p, "0 0 0") &&
      parse_coordinate_pair (p, &ex, &ey) &&
      parse_string (p, "z") &&
      *r == r1 && r1 == r2 && r2 == r3 &&
      *sx == ex && *sy == ey)
    {
      skip_whitespace (p);

      return TRUE;
    }

  *p = o;
  return FALSE;
}

/**
 * gsk_path_parse:
 * @string: a string
 *
 * This is a convenience function that constructs a `GskPath`
 * from a serialized form.
 *
 * The string is expected to be in
 * [SVG path syntax](https://www.w3.org/TR/SVG11/paths.html#PathData),
 * as e.g. produced by [method@Gsk.Path.to_string].
 *
 * Returns: (nullable): a new `GskPath`, or %NULL if @string could not be parsed
 **/
GskPath *
gsk_path_parse (const char *string)
{
  GskPathBuilder *builder;
  double x, y;
  double prev_x1, prev_y1;
  double path_x, path_y;
  const char *p;
  char cmd;
  char prev_cmd;
  gboolean after_comma;
  gboolean repeat;

  builder = gsk_path_builder_new ();

  cmd = 'X';
  path_x = path_y = 0;
  x = y = 0;
  prev_x1 = prev_y1 = 0;
  after_comma = FALSE;

  p = string;
  while (*p)
    {
      prev_cmd = cmd;
      repeat = !parse_command (&p, &cmd);

      if (after_comma && !repeat)
        goto error;

      switch (cmd)
        {
        case 'X':
          goto error;

        case 'Z':
        case 'z':
          if (repeat)
            goto error;
          else
            {
              gsk_path_builder_close (builder);
              x = path_x;
              y = path_y;
            }
          break;

        case 'M':
        case 'm':
          {
            double x1, y1, w, h, r;

            if (parse_rectangle (&p, &x1, &y1, &w, &h))
              {
                gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (x1, y1, w, h));
                if (strchr ("zZX", prev_cmd))
                  {
                    path_x = x1;
                    path_y = y1;
                  }
                x = x1;
                y = y1;
              }
            else if (parse_circle (&p, &x1, &y1, &r))
              {
                gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (x1 - r, y1), r);
                if (strchr ("zZX", prev_cmd))
                  {
                    path_x = x1;
                    path_y = y1;
                  }
                x = x1;
                y = y1;
              }
            else if (parse_coordinate_pair (&p, &x1, &y1))
              {
                if (cmd == 'm')
                  {
                    x1 += x;
                    y1 += y;
                  }
                if (repeat)
                  gsk_path_builder_line_to (builder, x1, y1);
                else
                  {
                    gsk_path_builder_move_to (builder, x1, y1);
                    if (strchr ("zZX", prev_cmd))
                      {
                        path_x = x1;
                        path_y = y1;
                      }
                  }
                x = x1;
                y = y1;
              }
            else
              goto error;
          }
          break;

        case 'L':
        case 'l':
          {
            double x1, y1;

            if (parse_coordinate_pair (&p, &x1, &y1))
              {
                if (cmd == 'l')
                  {
                    x1 += x;
                    y1 += y;
                  }

                if (strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_line_to (builder, x1, y1);
                x = x1;
                y = y1;
              }
            else
              goto error;
          }
          break;

        case 'H':
        case 'h':
          {
            double x1;

            if (parse_coordinate (&p, &x1))
              {
                if (cmd == 'h')
                  x1 += x;
                if (strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_line_to (builder, x1, y);
                x = x1;
              }
            else
              goto error;
          }
          break;

        case 'V':
        case 'v':
          {
            double y1;

            if (parse_coordinate (&p, &y1))
              {
                if (cmd == 'v')
                  y1 += y;
                if (strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_line_to (builder, x, y1);
                y = y1;
              }
            else
              goto error;
          }
          break;

        case 'C':
        case 'c':
          {
            double x0, y0, x1, y1, x2, y2;

            if (parse_coordinate_pair (&p, &x0, &y0) &&
                parse_coordinate_pair (&p, &x1, &y1) &&
                parse_coordinate_pair (&p, &x2, &y2))
              {
                if (cmd == 'c')
                  {
                    x0 += x;
                    y0 += y;
                    x1 += x;
                    y1 += y;
                    x2 += x;
                    y2 += y;
                  }
                if (strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_curve_to (builder, x0, y0, x1, y1, x2, y2);
                prev_x1 = x1;
                prev_y1 = y1;
                x = x2;
                y = y2;
              }
            else
              goto error;
          }
          break;

        case 'S':
        case 's':
          {
            double x0, y0, x1, y1, x2, y2;

            if (parse_coordinate_pair (&p, &x1, &y1) &&
                parse_coordinate_pair (&p, &x2, &y2))
              {
                if (cmd == 's')
                  {
                    x1 += x;
                    y1 += y;
                    x2 += x;
                    y2 += y;
                  }
                if (strchr ("CcSs", prev_cmd))
                  {
                    x0 = 2 * x - prev_x1;
                    y0 = 2 * y - prev_y1;
                  }
                else
                  {
                    x0 = x;
                    y0 = y;
                  }
                if (strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_curve_to (builder, x0, y0, x1, y1, x2, y2);
                prev_x1 = x1;
                prev_y1 = y1;
                x = x2;
                y = y2;
              }
            else
              goto error;
          }
          break;

        case 'Q':
        case 'q':
          {
            double x1, y1, x2, y2, xx1, yy1, xx2, yy2;

            if (parse_coordinate_pair (&p, &x1, &y1) &&
                parse_coordinate_pair (&p, &x2, &y2))
              {
                if (cmd == 'q')
                  {
                    x1 += x;
                    y1 += y;
                    x2 += x;
                    y2 += y;
                  }
                xx1 = (x + 2.0 * x1) / 3.0;
                yy1 = (y + 2.0 * y1) / 3.0;
                xx2 = (x2 + 2.0 * x1) / 3.0;
                yy2 = (y2 + 2.0 * y1) / 3.0;
                if (strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_curve_to (builder, xx1, yy1, xx2, yy2, x2, y2);
                prev_x1 = x1;
                prev_y1 = y1;
                x = x2;
                y = y2;
              }
            else
              goto error;
          }
          break;

        case 'T':
        case 't':
          {
            double x1, y1, x2, y2, xx1, yy1, xx2, yy2;

            if (parse_coordinate_pair (&p, &x2, &y2))
              {
                if (cmd == 't')
                  {
                    x2 += x;
                    y2 += y;
                  }
                if (strchr ("QqTt", prev_cmd))
                  {
                    x1 = 2 * x - prev_x1;
                    y1 = 2 * y - prev_y1;
                  }
                else
                  {
                    x1 = x;
                    y1 = y;
                  }
                xx1 = (x + 2.0 * x1) / 3.0;
                yy1 = (y + 2.0 * y1) / 3.0;
                xx2 = (x2 + 2.0 * x1) / 3.0;
                yy2 = (y2 + 2.0 * y1) / 3.0;
                if (strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_curve_to (builder, xx1, yy1, xx2, yy2, x2, y2);
                prev_x1 = x1;
                prev_y1 = y1;
                x = x2;
                y = y2;
              }
            else
              goto error;
          }
          break;

        case 'O':
        case 'o':
          {
            double x1, y1, x2, y2, weight;

            if (parse_coordinate_pair (&p, &x1, &y1) &&
                parse_coordinate_pair (&p, &x2, &y2) &&
                parse_nonnegative_number (&p, &weight))
              {
                if (cmd == 'c')
                  {
                    x1 += x;
                    y1 += y;
                    x2 += x;
                    y2 += y;
                  }
                if (strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_conic_to (builder, x1, y1, x2, y2, weight);
                x = x2;
                y = y2;
              }
            else
              goto error;
          }
          break;

        case 'A':
        case 'a':
          {
            double rx, ry;
            double x_axis_rotation;
            int large_arc, sweep;
            double x1, y1;

            if (parse_nonnegative_number (&p, &rx) &&
                parse_nonnegative_number (&p, &ry) &&
                parse_number (&p, &x_axis_rotation) &&
                parse_flag (&p, &large_arc) &&
                parse_flag (&p, &sweep) &&
                parse_coordinate_pair (&p, &x1, &y1))
              {
                if (cmd == 'a')
                  {
                    x1 += x;
                    y1 += y;
                  }

                if (strchr ("zZ", prev_cmd))
                  {
                    gsk_path_builder_move_to (builder, x, y);
                    path_x = x;
                    path_y = y;
                  }
                gsk_path_builder_svg_arc_to (builder,
                                             rx, ry, x_axis_rotation,
                                             large_arc, sweep,
                                             x1, y1);
                x = x1;
                y = y1;
              }
            else
              goto error;
          }
          break;

        default:
          goto error;
        }

      after_comma = (p > string) && p[-1] == ',';
    }

  if (after_comma)
    goto error;

  return gsk_path_builder_free_to_path (builder);

error:
  //g_warning ("Can't parse string '%s' as GskPath, error at %ld", string, p - string);
  gsk_path_builder_unref (builder);

  return NULL;
}

/**
 * gsk_path_get_stroke_bounds:
 * @path: a `GtkPath`
 * @stroke: stroke parameters
 * @bounds: (out caller-allocates): the bounds to fill in
 *
 * Computes the bounds for stroking the given path with the
 * parameters in @stroke.
 *
 * The returned bounds may be larger than necessary, because this
 * function aims to be fast, not accurate. The bounds are guaranteed
 * to contain the area affected by the stroke, including protrusions
 * like miters.
 *
 * Returns: %TRUE if the path has bounds, %FALSE if the path is known
 *   to be empty and have no bounds.
 */
gboolean
gsk_path_get_stroke_bounds (GskPath         *path,
                            const GskStroke *stroke,
                            graphene_rect_t *bounds)
{
  gsize i;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (bounds != NULL, FALSE);

  for (i = 0; i < path->n_contours; i++)
    {
      if (gsk_contour_get_stroke_bounds (path->contours[i], stroke, bounds))
        break;
    }

  if (i >= path->n_contours)
    {
      graphene_rect_init_from_rect (bounds, graphene_rect_zero ());
      return FALSE;
    }

  for (i++; i < path->n_contours; i++)
    {
      graphene_rect_t tmp;

      if (gsk_contour_get_stroke_bounds (path->contours[i], stroke, &tmp))
        graphene_rect_union (bounds, &tmp, bounds);
    }

  return TRUE;
}

/**
 * gsk_path_stroke:
 * @self: a `GskPath`
 * @stroke: stroke parameters
 *
 * Create a new path that follows the outline of the area
 * that would be affected by stroking along @self with
 * the given stroke parameters.
 *
 * Returns: a new `GskPath`
 */
GskPath *
gsk_path_stroke (GskPath   *self,
                 GskStroke *stroke)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();

  for (int i = 0; i < self->n_contours; i++)
    gsk_contour_add_stroke (gsk_path_get_contour (self, i), builder, stroke);

  return gsk_path_builder_free_to_path (builder);
}
