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

#include <glib.h>

G_BEGIN_DECLS

#ifndef OTTIE_KEYFRAMES_TYPE_NAME
#define OTTIE_KEYFRAMES_TYPE_NAME OttieKeyframes
#endif

#ifndef OTTIE_KEYFRAMES_NAME
#define OTTIE_KEYFRAMES_NAME ottie_keyframes
#endif

#ifndef OTTIE_KEYFRAMES_ELEMENT_TYPE
#define OTTIE_KEYFRAMES_ELEMENT_TYPE gpointer
#endif

#ifndef OTTIE_KEYFRAMES_DIMENSIONS
#define OTTIE_KEYFRAMES_DIMENSIONS 1
#endif

/* make this readable */
#define _T_ OTTIE_KEYFRAMES_ELEMENT_TYPE
#define OttieKeyframes OTTIE_KEYFRAMES_TYPE_NAME
#define OttieKeyframe OTTIE_KEYFRAMES_TYPE_NAME ## Keyframe
#define OttieControlPoint OTTIE_KEYFRAMES_TYPE_NAME ## ControlPoint
#define ottie_keyframes_paste_more(OTTIE_KEYFRAMES_NAME, func_name) OTTIE_KEYFRAMES_NAME ## _ ## func_name
#define ottie_keyframes_paste(OTTIE_KEYFRAMES_NAME, func_name) ottie_keyframes_paste_more (OTTIE_KEYFRAMES_NAME, func_name)
#define ottie_keyframes(func_name) ottie_keyframes_paste (OTTIE_KEYFRAMES_NAME, func_name)

typedef struct OttieControlPoint OttieControlPoint;
typedef struct OttieKeyframe OttieKeyframe;
typedef struct OttieKeyframes OttieKeyframes;

struct OttieControlPoint
{
  double x[OTTIE_KEYFRAMES_DIMENSIONS];
  double y[OTTIE_KEYFRAMES_DIMENSIONS];
};

struct OttieKeyframe
{
  /* Cubic control points, but Lottie names them in and out points */
  OttieControlPoint in;
  OttieControlPoint out;
  double start_time;
  _T_ value;
};

struct OttieKeyframes
{
  gsize n_items;
  OttieKeyframe items[];
};

static inline OttieKeyframes *
ottie_keyframes(new) (gsize n_items)
{
  OttieKeyframes *self;

  self = g_malloc0 (sizeof (OttieKeyframes) + n_items * sizeof (OttieKeyframe));
  self->n_items = n_items;

  return self;
}

static inline void
ottie_keyframes(free_item) (_T_ *item)
{
#ifdef OTTIE_KEYFRAMES_FREE_FUNC
#ifdef OTTIE_KEYFRAMES_BY_VALUE
    OTTIE_KEYFRAMES_FREE_FUNC (item);
#else
    OTTIE_KEYFRAMES_FREE_FUNC (*item);
#endif
#endif
}

/* no G_GNUC_UNUSED here */
static inline void
ottie_keyframes(free) (OttieKeyframes *self)
{
#ifdef OTTIE_KEYFRAMES_FREE_FUNC
  gsize i;

  for (i = 0; i < self->n_items; i++)
    ottie_keyframes(free_item) (&self->items[i].value);
#endif
}

static
#ifdef OTTIE_KEYFRAMES_BY_VALUE
void
#else
_T_
#endif
ottie_keyframes(get) (const OttieKeyframes *self,
                      double                timestamp
#ifdef OTTIE_KEYFRAMES_BY_VALUE
                      , _T_                *out_result
#endif
                      )
{
  const OttieKeyframe *start, *end;

  start = end = NULL;

  for (gsize i = 0; i < self->n_items; i++)
    {
      if (self->items[i].start_time <= timestamp)
        start = &self->items[i];

      if (self->items[i].start_time >= timestamp)
        {
          end = &self->items[i];
          break;
        }
    }

  g_assert (start != NULL || end != NULL);

#ifdef OTTIE_KEYFRAMES_BY_VALUE
  if (start == NULL || start == end)
    *out_result = end->value;
  else if (end == NULL)
    *out_result = start->value;
#else
  if (start == NULL || start == end)
    return end->value;
  else if (end == NULL)
    return start->value;
#endif
  else
    {
      double progress = (timestamp - start->start_time) / (end->start_time - start->start_time);
#ifdef OTTIE_KEYFRAMES_BY_VALUE
      OTTIE_KEYFRAMES_INTERPOLATE_FUNC (&start->value, &end->value, progress, out_result);
#else
      return OTTIE_KEYFRAMES_INTERPOLATE_FUNC (start->value, end->value, progress);
#endif
    }
}

static gboolean
ottie_keyframes(parse_control_point_dimension) (JsonReader *reader,
                                                gsize       offset,
                                                gpointer    data)
{
  double d[OTTIE_KEYFRAMES_DIMENSIONS];

  if (json_reader_is_array (reader))
    {
      if (json_reader_count_elements (reader) != OTTIE_KEYFRAMES_DIMENSIONS)
        ottie_parser_error_value (reader, "control point has %d dimension, not %u", json_reader_count_elements (reader), OTTIE_KEYFRAMES_DIMENSIONS);

      for (int i = 0; i < OTTIE_KEYFRAMES_DIMENSIONS; i++)
        {
          if (!json_reader_read_element (reader, i))
            {
              ottie_parser_emit_error (reader, json_reader_get_error (reader));
            }
          else
            {
              if (!ottie_parser_option_double (reader, 0, &d[i]))
                d[i] = 0;
            }
          json_reader_end_element (reader);
        }
    }
  else
    {
      if (!ottie_parser_option_double (reader, 0, &d[0]))
        return FALSE;

      for (gsize i = 1; i < OTTIE_KEYFRAMES_DIMENSIONS; i++)
        d[i] = d[0];
    }

  memcpy ((guint8 *) data + GPOINTER_TO_SIZE (offset), d, sizeof (double) * OTTIE_KEYFRAMES_DIMENSIONS);
  return TRUE;
}

static gboolean
ottie_keyframes(parse_control_point) (JsonReader *reader,
                                      gsize       offset,
                                      gpointer    data)
{
  OttieParserOption options[] = {
    { "x", ottie_keyframes(parse_control_point_dimension), G_STRUCT_OFFSET (OttieControlPoint, x) },
    { "y", ottie_keyframes(parse_control_point_dimension), G_STRUCT_OFFSET (OttieControlPoint, y) },
  };
  OttieControlPoint cp;
  OttieControlPoint *target = (OttieControlPoint *) ((guint8 *) data + offset);

  if (!ottie_parser_parse_object (reader, "control point", options, G_N_ELEMENTS (options), &cp))
    return FALSE;

  *target = cp;
  return TRUE;
}

static gboolean
ottie_keyframes(parse_keyframe) (JsonReader *reader,
                                 gsize       offset,
                                 gpointer    data)
{
  OttieParserOption options[] = {
    { "s", OTTIE_KEYFRAMES_PARSE_FUNC, G_STRUCT_OFFSET (OttieKeyframe, value) },
    { "t", ottie_parser_option_double, G_STRUCT_OFFSET (OttieKeyframe, start_time) },
    { "i", ottie_keyframes(parse_control_point), G_STRUCT_OFFSET (OttieKeyframe, in) },
    { "o", ottie_keyframes(parse_control_point), G_STRUCT_OFFSET (OttieKeyframe, out) },
    { "ix",  ottie_parser_option_skip_index, 0 },
  };
  OttieKeyframe *keyframe = (OttieKeyframe *) ((guint8 *) data + offset);
      
  return ottie_parser_parse_object (reader, "keyframe", options, G_N_ELEMENTS (options), keyframe);
}

/* no G_GNUC_UNUSED here, if you don't use a type, remove it. */
static inline OttieKeyframes *
ottie_keyframes(parse) (JsonReader *reader)
{
  OttieKeyframes *self;

  self = ottie_keyframes(new) (json_reader_count_elements (reader));

  if (!ottie_parser_parse_array (reader, "keyframes",
                                 self->n_items, self->n_items,
                                 NULL,
                                 G_STRUCT_OFFSET (OttieKeyframes, items),
                                 sizeof (OttieKeyframe),
                                 ottie_keyframes(parse_keyframe),
                                 self))
    {
      ottie_keyframes(free) (self);
      return NULL;
    }

  /* XXX: Do we need to order keyframes here? */

  return self;
}

#ifndef OTTIE_KEYFRAMES_NO_UNDEF

#undef _T_
#undef OttieKeyframes
#undef ottie_keyframes_paste_more
#undef ottie_keyframes_paste
#undef ottie_keyframes

#undef OTTIE_KEYFRAMES_PARSE_FUNC
#undef OTTIE_KEYFRAMES_BY_VALUE
#undef OTTIE_KEYFRAMES_ELEMENT_TYPE
#undef OTTIE_KEYFRAMES_FREE_FUNC
#undef OTTIE_KEYFRAMES_NAME
#undef OTTIE_KEYFRAMES_TYPE_NAME
#endif
