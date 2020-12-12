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

#include "ottiepathvalueprivate.h"

#include "ottieparserprivate.h"

#include <glib/gi18n-lib.h>

typedef struct _OttieContour OttieContour;
typedef struct _OttieCurve OttieCurve;
typedef struct _OttiePath OttiePath;

struct _OttieCurve {
  double point[2];
  double in[2];
  double out[2];
};

struct _OttieContour {
  gboolean closed;
  guint n_curves;
  OttieCurve curves[0];
};

struct _OttiePath {
  guint ref_count;
  guint n_contours;
  OttieContour *contours[];
};

static OttieContour *
ottie_contour_renew (OttieContour *path,
                     guint         n_curves)
{
  OttieContour *self;

  self = g_realloc (path, sizeof (OttieContour) + n_curves * sizeof (OttieCurve));
  self->n_curves = n_curves;

  return self;
}

static OttieContour *
ottie_contour_new (gboolean closed,
                   guint    n_curves)
{
  OttieContour *self;

  self = g_malloc0 (sizeof (OttieContour) + n_curves * sizeof (OttieCurve));
  self->closed = closed;
  self->n_curves = n_curves;

  return self;
}

static void
ottie_contour_free (OttieContour *path)
{
  g_free (path);
}

static gboolean
ottie_parse_value_parse_numbers (JsonReader *reader,
                                 gsize       offset,
                                 gpointer    data)
{
  return ottie_parser_parse_array (reader, "number",
                                   2, 2, NULL,
                                   offset, sizeof (double),
                                   ottie_parser_option_double, data);
}

#define MAKE_OPEN_CONTOUR (NULL)
#define MAKE_CLOSED_CONTOUR GSIZE_TO_POINTER(1)
static gboolean
ottie_parse_value_parse_curve_array (JsonReader *reader,
                                     gsize       offset,
                                     gpointer    data)
{
  /* Attention: The offset value here is to the point in the curve, not 
   * to the target */
  OttieContour **target = (OttieContour **) data;
  OttieContour *path = *target;
  guint n_curves;

  n_curves = json_reader_count_elements (reader);
  if (path == MAKE_OPEN_CONTOUR)
    path = ottie_contour_new (FALSE, n_curves);
  else if (path == MAKE_CLOSED_CONTOUR)
    path = ottie_contour_new (TRUE, n_curves);
  else if (n_curves < path->n_curves)
    path = ottie_contour_renew (path, n_curves);
  else if (n_curves > path->n_curves)
    n_curves = path->n_curves;

  *target = path;

  return ottie_parser_parse_array (reader, "path array",
                                   0, path->n_curves, NULL,
                                   offset, sizeof (OttieCurve),
                                   ottie_parse_value_parse_numbers, &path->curves[0]);
}

static gboolean
ottie_parser_value_parse_closed (JsonReader *reader,
                                 gsize       offset,
                                 gpointer    data)
{
  OttieContour **target = (OttieContour **) data;
  gboolean b;

  b = json_reader_get_boolean_value (reader);
  if (json_reader_get_error (reader))
    {
      ottie_parser_emit_error (reader, json_reader_get_error (reader));
      return FALSE;
    }
  
  if (*target == MAKE_OPEN_CONTOUR || *target == MAKE_CLOSED_CONTOUR)
    {
      if (b)
        *target = MAKE_CLOSED_CONTOUR;
      else
        *target = MAKE_OPEN_CONTOUR;
    }
  else
    (*target)->closed = b;

  return TRUE;
}

static gboolean
ottie_path_value_parse_contour (JsonReader *reader,
                                gsize       offset,
                                gpointer    data)
{
  OttieContour **target = (OttieContour **) ((guint8 *) data + offset);
  OttieParserOption options[] = {
    { "c", ottie_parser_value_parse_closed, 0 },
    { "i", ottie_parse_value_parse_curve_array, G_STRUCT_OFFSET (OttieCurve, in) },
    { "o", ottie_parse_value_parse_curve_array, G_STRUCT_OFFSET (OttieCurve, out) },
    { "v", ottie_parse_value_parse_curve_array, G_STRUCT_OFFSET (OttieCurve, point) },
  };

  g_assert (*target == NULL);
  *target = MAKE_CLOSED_CONTOUR;

  if (!ottie_parser_parse_object (reader, "contour", options, G_N_ELEMENTS (options), target))
    {
      if (*target != MAKE_OPEN_CONTOUR && *target != MAKE_CLOSED_CONTOUR)
        g_clear_pointer (target, ottie_contour_free);
      *target = NULL;
      return FALSE;
    }

  if (*target == MAKE_OPEN_CONTOUR)
    *target = ottie_contour_new (FALSE, 0);
  else if (*target == MAKE_CLOSED_CONTOUR)
    *target = ottie_contour_new (TRUE, 0);

  return TRUE;
}

static OttiePath *
ottie_path_new (gsize n_contours)
{
  OttiePath *self;

  self = g_malloc0 (sizeof (OttiePath) + sizeof (OttieContour *) * n_contours);
  self->ref_count = 1;
  self->n_contours = n_contours;

  return self;
}

static OttiePath *
ottie_path_ref (OttiePath *self)
{
  self->ref_count++;

  return self;
}

static void
ottie_path_unref (OttiePath *self)
{
  self->ref_count--;
  if (self->ref_count > 0)
    return;

  for (guint i = 0; i < self->n_contours; i++)
    {
      g_clear_pointer (&self->contours[i], ottie_contour_free);
    }

  g_free (self);
}

static gboolean
ottie_path_value_parse_one (JsonReader *reader,
                            gsize       offset,
                            gpointer    data)
{
  OttiePath **target = (OttiePath **) ((guint8 *) data + offset);
  OttiePath *path;

  if (json_reader_is_array (reader))
    path = ottie_path_new (json_reader_count_elements (reader));
  else
    path = ottie_path_new (1);

  if (!ottie_parser_parse_array (reader, "path",
                                 path->n_contours, path->n_contours, NULL,
                                 G_STRUCT_OFFSET (OttiePath, contours),
                                 sizeof (OttieContour *),
                                 ottie_path_value_parse_contour, path))
    {
      ottie_path_unref (path);
      return FALSE;
    }

  g_clear_pointer (target, ottie_path_unref);
  *target = path;

  return TRUE;
}

static OttieContour *
ottie_contour_interpolate (const OttieContour *start,
                           const OttieContour *end,
                           double              progress)
{
  OttieContour *self = ottie_contour_new (start->closed || end->closed,
                                          MIN (start->n_curves, end->n_curves));

  for (gsize i = 0; i < self->n_curves; i++)
    {
      self->curves[i].point[0] = start->curves[i].point[0] + progress * (end->curves[i].point[0] - start->curves[i].point[0]);
      self->curves[i].point[1] = start->curves[i].point[1] + progress * (end->curves[i].point[1] - start->curves[i].point[1]);
      self->curves[i].in[0] = start->curves[i].in[0] + progress * (end->curves[i].in[0] - start->curves[i].in[0]);
      self->curves[i].in[1] = start->curves[i].in[1] + progress * (end->curves[i].in[1] - start->curves[i].in[1]);
      self->curves[i].out[0] = start->curves[i].out[0] + progress * (end->curves[i].out[0] - start->curves[i].out[0]);
      self->curves[i].out[1] = start->curves[i].out[1] + progress * (end->curves[i].out[1] - start->curves[i].out[1]);
    }

  return self;
}

static OttiePath *
ottie_path_interpolate (const OttiePath *start,
                        const OttiePath *end,
                        double           progress)
{
  OttiePath *self = ottie_path_new (MIN (start->n_contours, end->n_contours));

  for (gsize i = 0; i < self->n_contours; i++)
    {
      self->contours[i] = ottie_contour_interpolate (start->contours[i],
                                                     end->contours[i],
                                                     progress);
    }

  return self;
}

#define OTTIE_KEYFRAMES_NAME ottie_path_keyframes
#define OTTIE_KEYFRAMES_TYPE_NAME OttieContourKeyframes
#define OTTIE_KEYFRAMES_ELEMENT_TYPE OttiePath *
#define OTTIE_KEYFRAMES_COPY_FUNC ottie_path_ref
#define OTTIE_KEYFRAMES_FREE_FUNC ottie_path_unref
#define OTTIE_KEYFRAMES_PARSE_FUNC ottie_path_value_parse_one
#define OTTIE_KEYFRAMES_INTERPOLATE_FUNC ottie_path_interpolate
#include "ottiekeyframesimpl.c"

void
ottie_path_value_init (OttiePathValue *self)
{
  self->is_static = TRUE;
  self->static_value = NULL;
}

void
ottie_path_value_clear (OttiePathValue *self)
{
  if (self->is_static)
    g_clear_pointer (&self->static_value, ottie_path_unref);
  else
    g_clear_pointer (&self->keyframes, ottie_path_keyframes_free);
}

static GskPath *
ottie_path_build (OttiePath *self,
                  gboolean   reverse)
{
  GskPathBuilder *builder;

  if (reverse)
    g_warning ("FIXME: Make paths reversible");

  builder = gsk_path_builder_new ();
  for (gsize i = 0; i < self->n_contours; i++)
    {
      OttieContour *contour = self->contours[i];
      if (contour->n_curves == 0)
        continue;

      gsk_path_builder_move_to (builder,
                                contour->curves[0].point[0], contour->curves[0].point[1]);
      for (guint j = 1; j < contour->n_curves; j++)
        {
          gsk_path_builder_curve_to (builder,
                                     contour->curves[j-1].point[0] + contour->curves[j-1].out[0],
                                     contour->curves[j-1].point[1] + contour->curves[j-1].out[1],
                                     contour->curves[j].point[0] + contour->curves[j].in[0],
                                     contour->curves[j].point[1] + contour->curves[j].in[1],
                                     contour->curves[j].point[0],
                                     contour->curves[j].point[1]);
        }
      if (contour->closed)
        {
          gsk_path_builder_curve_to (builder,
                                     contour->curves[contour->n_curves-1].point[0] + contour->curves[contour->n_curves-1].out[0],
                                     contour->curves[contour->n_curves-1].point[1] + contour->curves[contour->n_curves-1].out[1],
                                     contour->curves[0].point[0] + contour->curves[0].in[0],
                                     contour->curves[0].point[1] + contour->curves[0].in[1],
                                     contour->curves[0].point[0],
                                     contour->curves[0].point[1]);
          gsk_path_builder_close (builder);
        }
    }

  return gsk_path_builder_free_to_path (builder);
}

GskPath *
ottie_path_value_get (OttiePathValue *self,
                      double          timestamp,
                      gboolean        reverse)
{
  if (self->is_static)
    return ottie_path_build (self->static_value, reverse);
  
  return ottie_path_build (ottie_path_keyframes_get (self->keyframes, timestamp), reverse);
}

gboolean
ottie_path_value_parse (JsonReader *reader,
                        gsize       offset,
                        gpointer    data)
{
  OttiePathValue *self = (OttiePathValue *) ((guint8 *) data + GPOINTER_TO_SIZE (offset));

  if (json_reader_read_member (reader, "k"))
    {
      if (!json_reader_is_array (reader))
        {
          if (!ottie_path_value_parse_one (reader, 0, &self->static_value))
            {
              json_reader_end_member (reader);
              return FALSE;
            }
          self->is_static = TRUE;
        }
      else
        {
          self->keyframes = ottie_path_keyframes_parse (reader);
          if (self->keyframes == NULL)
            {
              json_reader_end_member (reader);
              return FALSE;
            }
          self->is_static = FALSE;
        }
    }
  else
    {
      ottie_parser_error_syntax (reader, "Property is not a path value");
    }
  json_reader_end_member (reader);

  return TRUE;
}

