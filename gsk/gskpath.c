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

#include "gskpath.h"


typedef struct _GskContour GskContour;
typedef struct _GskContourClass GskContourClass;

struct _GskContour
{
  const GskContourClass *klass;
};

struct _GskContourClass
{
  gsize struct_size;
  const char *type_name;

  gsize                 (* get_size)            (const GskContour       *contour);
  void                  (* print)               (const GskContour       *contour,
                                                 GString                *string);
  void                  (* to_cairo)            (const GskContour       *contour,
                                                 cairo_t                *cr);
  gboolean              (* get_bounds)          (const GskContour       *contour,
                                                 graphene_rect_t        *bounds);
};

struct _GskPath
{
  /*< private >*/
  guint ref_count;

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

static gsize
gsk_contour_get_size_default (const GskContour *contour)
{
  return contour->klass->struct_size;
}

/* RECT CONTOUR */

typedef struct _GskRectContour GskRectContour;
struct _GskRectContour
{
  GskContour contour;

  float x;
  float y;
  float width;
  float height;
};

static void
_g_string_append_double (GString *string,
                         double   d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, d);
  g_string_append (string, buf);
}

static void
_g_string_append_point (GString                *string,
                        const graphene_point_t *pt)
{
  _g_string_append_double (string, pt->x);
  g_string_append_c (string, ' ');
  _g_string_append_double (string, pt->y);
}

static void
gsk_rect_contour_print (const GskContour *contour,
                        GString          *string)
{
  const GskRectContour *self = (const GskRectContour *) contour;

  g_string_append (string, "M ");
  _g_string_append_point (string, &GRAPHENE_POINT_INIT (self->x, self->y));
  g_string_append (string, " h ");
  _g_string_append_double (string, self->width);
  g_string_append (string, " v ");
  _g_string_append_double (string, self->height);
  g_string_append (string, " h ");
  _g_string_append_double (string, - self->width);
  g_string_append (string, " z");
}

static void
gsk_rect_contour_to_cairo (const GskContour *contour,
                           cairo_t          *cr)
{
  const GskRectContour *self = (const GskRectContour *) contour;

  cairo_rectangle (cr,
                   self->x, self->y,
                   self->width, self->height);
}

static gboolean
gsk_rect_contour_get_bounds (const GskContour *contour,
                             graphene_rect_t  *rect)
{
  const GskRectContour *self = (const GskRectContour *) contour;

  graphene_rect_init (rect, self->x, self->y, self->width, self->height);

  return TRUE;
}

static const GskContourClass GSK_RECT_CONTOUR_CLASS =
{
  sizeof (GskRectContour),
  "GskRectContour",
  gsk_contour_get_size_default,
  gsk_rect_contour_print,
  gsk_rect_contour_to_cairo,
  gsk_rect_contour_get_bounds
};

static void
gsk_rect_contour_init (GskContour *contour,
                       float       x,
                       float       y,
                       float       width,
                       float       height)
{
  GskRectContour *self = (GskRectContour *) contour;

  self->contour.klass = &GSK_RECT_CONTOUR_CLASS;
  self->x = x;
  self->y = y;
  self->width = width;
  self->height = height;
}

/* STANDARD CONTOUR */

typedef enum
{
  GSK_PATH_VERB_MOVE,
  GSK_PATH_VERB_CLOSE,
  GSK_PATH_VERB_LINE,
  GSK_PATH_VERB_CURVE,
} GskPathVerb;

typedef union {
  GskPathVerb verb;
  graphene_point_t point;
} GskStandardPoint;

typedef struct _GskStandardContour GskStandardContour;
struct _GskStandardContour
{
  GskContour contour;

  gsize n_points;
  GskStandardPoint path[];
};

static gsize
gsk_path_verb_n_elements (GskPathVerb verb)
{
  switch (verb)
  {
    case GSK_PATH_VERB_MOVE:
    case GSK_PATH_VERB_LINE:
    case GSK_PATH_VERB_CLOSE:
      return 2;

    case GSK_PATH_VERB_CURVE:
      return 4;

    default:
      g_assert_not_reached();
      return 0;
  }
}

static gsize
gsk_standard_contour_get_size (const GskContour *contour)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;

  return sizeof (GskStandardContour) + sizeof (GskStandardPoint) * self->n_points;
}

static void
gsk_standard_contour_print (const GskContour *contour,
                            GString          *string)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;
  gsize i;

  for (i = 0; i < self->n_points; i += gsk_path_verb_n_elements (self->path[i].verb))
    {
      switch (self->path[i].verb)
      {
        case GSK_PATH_VERB_MOVE:
          g_string_append (string, "M ");
          _g_string_append_point (string, &self->path[i + 1].point);
          break;

        case GSK_PATH_VERB_CLOSE:
          g_string_append (string, " Z");
          break;

        case GSK_PATH_VERB_LINE:
          g_string_append (string, " L ");
          _g_string_append_point (string, &self->path[i + 1].point);
          break;

        case GSK_PATH_VERB_CURVE:
          g_string_append (string, " C ");
          _g_string_append_point (string, &self->path[i + 1].point);
          g_string_append (string, ", ");
          _g_string_append_point (string, &self->path[i + 2].point);
          g_string_append (string, ", ");
          _g_string_append_point (string, &self->path[i + 3].point);
          break;

        default:
          g_assert_not_reached();
          return;
      }
    }
}

static void
gsk_standard_contour_to_cairo (const GskContour *contour,
                               cairo_t          *cr)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;
  gsize i;

  cairo_new_sub_path (cr);

  for (i = 0; i < self->n_points; i += gsk_path_verb_n_elements (self->path[i].verb))
    {
      switch (self->path[i].verb)
      {
        case GSK_PATH_VERB_MOVE:
          cairo_move_to (cr, self->path[i + 1].point.x, self->path[i + 1].point.y);
          break;

        case GSK_PATH_VERB_CLOSE:
          cairo_close_path (cr);
          break;

        case GSK_PATH_VERB_LINE:
          cairo_line_to (cr, self->path[i + 1].point.x, self->path[i + 1].point.y);
          break;

        case GSK_PATH_VERB_CURVE:
          cairo_curve_to (cr,
                          self->path[i + 1].point.x, self->path[i + 1].point.y,
                          self->path[i + 2].point.x, self->path[i + 2].point.y,
                          self->path[i + 3].point.x, self->path[i + 3].point.y);
          break;

        default:
          g_assert_not_reached();
          return;
      }
    }
}

static void
rect_add_point (graphene_rect_t        *rect,
                const graphene_point_t *point)
{
  if (point->x < rect->origin.x)
    {
      rect->size.width += rect->origin.x - point->x;
      rect->origin.x = point->x;
    }
  else if (point->x > rect->origin.x + rect->size.width)
    {
      rect->size.width = point->x - rect->origin.x;
    }

  if (point->y < rect->origin.y)
    {
      rect->size.height += rect->origin.y - point->y;
      rect->origin.y = point->y;
    }
  else if (point->y > rect->origin.y + rect->size.height)
    {
      rect->size.height = point->y - rect->origin.y;
    }
}

static gboolean
gsk_standard_contour_get_bounds (const GskContour *contour,
                                 graphene_rect_t  *bounds)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;
  gsize i;

  for (i = 0; i < self->n_points; i += gsk_path_verb_n_elements (self->path[i].verb))
    {
      switch (self->path[i].verb)
      {
        case GSK_PATH_VERB_MOVE:
          graphene_rect_init (bounds,
                              self->path[i + 1].point.x, self->path[i + 1].point.y,
                              0, 0);
          break;

        case GSK_PATH_VERB_CLOSE:
          break;

        case GSK_PATH_VERB_LINE:
          rect_add_point (bounds, &self->path[i + 1].point);
          break;

        case GSK_PATH_VERB_CURVE:
          rect_add_point (bounds, &self->path[i + 1].point);
          rect_add_point (bounds, &self->path[i + 2].point);
          rect_add_point (bounds, &self->path[i + 3].point);
          break;

        default:
          g_assert_not_reached();
          return FALSE;
      }
    }

  return bounds->size.width > 0 && bounds->size.height > 0;
}

static const GskContourClass GSK_STANDARD_CONTOUR_CLASS =
{
  sizeof (GskStandardContour),
  "GskStandardContour",
  gsk_standard_contour_get_size,
  gsk_standard_contour_print,
  gsk_standard_contour_to_cairo,
  gsk_standard_contour_get_bounds
};

static void
gsk_standard_contour_init (GskContour *contour)
{
  contour->klass = &GSK_STANDARD_CONTOUR_CLASS;
}

/* PATH */

static GskPath *
gsk_path_alloc (gsize extra_size)
{
  GskPath *self;

  self = g_malloc0 (sizeof (GskPath) + extra_size);
  self->ref_count = 1;
  self->n_contours = 0;

  return self;
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
      self->contours[i]->klass->print (self->contours[i], string);
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
  gsize i;

  g_return_if_fail (self != NULL);
  g_return_if_fail (cr != NULL);

  for (i = 0; i < self->n_contours; i++)
    {
      self->contours[i]->klass->to_cairo (self->contours[i], cr);
    }
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
      if (self->contours[i]->klass->get_bounds (self->contours[i], bounds))
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

      if (self->contours[i]->klass->get_bounds (self->contours[i], &tmp))
        graphene_rect_union (bounds, &tmp, bounds);
    }

  return TRUE;
}

/**
 * GskPathBuilder:
 *
 * A `GskPathBuilder` struct is an opaque struct. It is meant to
 * not be kept around and only be used to create new `GskPath`
 * objects.
 */

struct _GskPathBuilder
{
  int ref_count;

  GSList *contours;
  GskStandardContour *current;
};

G_DEFINE_BOXED_TYPE (GskPathBuilder,
                     gsk_path_builder,
                     gsk_path_builder_ref,
                     gsk_path_builder_unref)


/**
 * gsk_path_builder_new:
 *
 * Create a new `GskPathBuilder` object. The resulting builder
 * would create an empty `GskPath`. Use addition functions to add
 * types to it.
 *
 * Returns: a new `GskPathBuilder`
 **/
GskPathBuilder *
gsk_path_builder_new (void)
{
  GskPathBuilder *builder;

  builder = g_slice_new0 (GskPathBuilder);
  builder->ref_count = 1;

  return builder;
}

/**
 * gsk_path_builder_ref:
 * @builder: a `GskPathBuilder`
 *
 * Acquires a reference on the given @builder.
 *
 * This function is intended primarily for bindings. `GskPathBuilder` objects
 * should not be kept around.
 *
 * Returns: (transfer none): the given `GskPathBuilder` with
 *   its reference count increased
 */
GskPathBuilder *
gsk_path_builder_ref (GskPathBuilder *builder)
{
  g_return_val_if_fail (builder != NULL, NULL);
  g_return_val_if_fail (builder->ref_count > 0, NULL);

  builder->ref_count += 1;

  return builder;
}

static void
gsk_path_builder_append_current (GskPathBuilder   *builder,
                                 gsize             n_points,
                                 GskStandardPoint *points)
{
  gsize size;

  g_assert (builder->current);

  size = sizeof (GskStandardContour) + sizeof (GskStandardPoint) * builder->current->n_points;
  builder->current = g_realloc (builder->current, size + sizeof (GskStandardPoint) * n_points);
  memcpy ((guint8 *) builder->current + size, points, sizeof (GskStandardPoint) * n_points);
  builder->current->n_points += n_points;
}

static void
gsk_path_builder_end_current (GskPathBuilder *builder)
{
  if (builder->current == NULL)
   return;

  builder->contours = g_slist_prepend (builder->contours, builder->current);
  builder->current = NULL;
}

static void
gsk_path_builder_clear (GskPathBuilder *builder)
{
  gsk_path_builder_end_current (builder);

  g_slist_free_full (builder->contours, g_free);
  builder->contours = NULL;
}

/**
 * gsk_path_builder_unref:
 * @builder: a `GskPathBuilder`
 *
 * Releases a reference on the given @builder.
 */
void
gsk_path_builder_unref (GskPathBuilder *builder)
{
  g_return_if_fail (builder != NULL);
  g_return_if_fail (builder->ref_count > 0);

  builder->ref_count -= 1;

  if (builder->ref_count > 0)
    return;

  gsk_path_builder_clear (builder);
  g_slice_free (GskPathBuilder, builder);
}

/**
 * gsk_path_builder_free_to_path: (skip)
 * @builder: a `GskPathBuilder`
 *
 * Creates a new `GskPath` from the current state of the
 * given @builder, and frees the @builder instance.
 *
 * Returns: (transfer full): the newly created `GskPath`
 *   with all the contours added to @builder
 */
GskPath *
gsk_path_builder_free_to_path (GskPathBuilder *builder)
{
  GskPath *res;

  g_return_val_if_fail (builder != NULL, NULL);

  res = gsk_path_builder_to_path (builder);

  gsk_path_builder_unref (builder);

  return res;
}

/**
 * gsk_path_builder_to_path:
 * @builder: a `GskPathBuilder`
 *
 * Creates a new `GskPath` from the given @builder.
 *
 * The given `GskPathBuilder` is reset once this function returns;
 * you cannot call this function multiple times on the same @builder instance.
 *
 * This function is intended primarily for bindings. C code should use
 * gsk_path_builder_free_to_path().
 *
 * Returns: (transfer full): the newly created `GskPath`
 *   with all the contours added to @builder
 */
GskPath *
gsk_path_builder_to_path (GskPathBuilder *builder)
{
  GskPath *path;
  GSList *l;
  gsize size;
  gsize n_contours;
  guint8 *contour_data;

  g_return_val_if_fail (builder != NULL, NULL);

  gsk_path_builder_end_current (builder);

  builder->contours = g_slist_reverse (builder->contours);
  size = 0;
  n_contours = 0;
  for (l = builder->contours; l; l = l->next)
    {
      GskContour *contour = l->data;

      n_contours++;
      size += sizeof (GskContour *);
      size += contour->klass->get_size (contour);
    }

  path = gsk_path_alloc (size);
  path->n_contours = n_contours;
  contour_data = (guint8 *) &path->contours[n_contours];
  n_contours = 0;

  for (l = builder->contours; l; l = l->next)
    {
      GskContour *contour = l->data;

      path->contours[n_contours] = (GskContour *) contour_data;
      size = contour->klass->get_size (contour);
      memcpy (contour_data, contour, size);
      contour_data += size;
      n_contours++;
    }

  gsk_path_builder_clear (builder);

  return path;
}

static GskContour *
gsk_path_builder_add_contour (GskPathBuilder        *builder,
                              const GskContourClass *klass)
{
  GskContour *contour;

  gsk_path_builder_end_current (builder);

  contour = g_malloc0 (klass->struct_size);
  builder->contours = g_slist_prepend (builder->contours, contour);

  return contour;
}

/**
 * gsk_path_builder_add_rect:
 * @builder: A `GskPathBuilder`
 * @rect: The rectangle to create a path for
 *
 * Creates a path representing the given rectangle.
 *
 * If the width or height of the rectangle is negative, the start
 * point will be on the right or bottom, respectively.
 *
 * If the the width or height are 0, the path will be a closed
 * horizontal or vertical line. If both are 0, it'll be a closed dot.
 *
 * Returns: a new `GskPath` representing a rectangle
 **/
void
gsk_path_builder_add_rect (GskPathBuilder        *builder,
                           const graphene_rect_t *rect)
{
  GskContour *contour;

  g_return_if_fail (builder != NULL);

  contour = gsk_path_builder_add_contour (builder, &GSK_RECT_CONTOUR_CLASS);
  gsk_rect_contour_init (contour,
                         rect->origin.x, rect->origin.y,
                         rect->size.width, rect->size.height);
}

void
gsk_path_builder_move_to (GskPathBuilder *builder,
                          float           x,
                          float           y)
{
  gsize size;

  g_return_if_fail (builder != NULL);

  gsk_path_builder_end_current (builder);

  size = GSK_STANDARD_CONTOUR_CLASS.struct_size + 2 * sizeof (GskStandardPoint);
  builder->current = g_malloc0 (size);
  gsk_standard_contour_init ((GskContour *) builder->current);
  builder->current->n_points = 2;
  builder->current->path[0].verb = GSK_PATH_VERB_MOVE;
  builder->current->path[1].point = GRAPHENE_POINT_INIT (x, y);
}

void
gsk_path_builder_line_to (GskPathBuilder *builder,
                          float           x,
                          float           y)
{
  g_return_if_fail (builder != NULL);

  if (builder->current == NULL)
    {
      gsk_path_builder_move_to (builder, x, y);
      return;
    }

  gsk_path_builder_append_current (builder, 2, (GskStandardPoint[2]) {
                                     { .verb = GSK_PATH_VERB_LINE },
                                     { .point = GRAPHENE_POINT_INIT (x, y) }
                                   });
}

void
gsk_path_builder_curve_to (GskPathBuilder *builder,
                           float           x1,
                           float           y1,
                           float           x2,
                           float           y2,
                           float           x3,
                           float           y3)
{
  g_return_if_fail (builder != NULL);

  if (builder->current == NULL)
    gsk_path_builder_move_to (builder, x1, y1);

  gsk_path_builder_append_current (builder, 4, (GskStandardPoint[4]) {
                                     { .verb = GSK_PATH_VERB_CURVE },
                                     { .point = GRAPHENE_POINT_INIT (x1, y1) },
                                     { .point = GRAPHENE_POINT_INIT (x2, y2) },
                                     { .point = GRAPHENE_POINT_INIT (x3, y3) }
                                   });
}

void
gsk_path_builder_close (GskPathBuilder *builder)
{
  g_return_if_fail (builder != NULL);

  if (builder->current == NULL)
    return;

  gsk_path_builder_append_current (builder, 2, (GskStandardPoint[2]) {
                                     { .verb = GSK_PATH_VERB_CLOSE },
                                     { .point = builder->current->path[1].point },
                                   });
  gsk_path_builder_end_current (builder);
}

