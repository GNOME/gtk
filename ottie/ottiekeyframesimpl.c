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
  _T_ start_value;
  _T_ end_value;
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
ottie_keyframes(free_value) (_T_ *item)
{
#ifdef OTTIE_KEYFRAMES_FREE_FUNC
#ifdef OTTIE_KEYFRAMES_BY_VALUE
    OTTIE_KEYFRAMES_FREE_FUNC (item);
#else
    OTTIE_KEYFRAMES_FREE_FUNC (*item);
#endif
#endif
}

static inline void
ottie_keyframes(copy_value) (_T_ *dest,
                             _T_ *src)
{
#ifdef OTTIE_KEYFRAMES_COPY_FUNC
#  ifdef OTTIE_KEYFRAMES_BY_VALUE
  OTTIE_KEYFRAMES_COPY_FUNC (dest, src);
#  else
  *dest = OTTIE_KEYFRAMES_COPY_FUNC (*src);
#  endif
#else
    *dest = *src;
#endif
}

/* no G_GNUC_UNUSED here */
static inline void
ottie_keyframes(free) (OttieKeyframes *self)
{
#ifdef OTTIE_KEYFRAMES_FREE_FUNC
  gsize i;

  for (i = 0; i < self->n_items; i++)
    {
      ottie_keyframes(free_value) (&self->items[i].start_value);
      ottie_keyframes(free_value) (&self->items[i].end_value);
    }
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
  const OttieKeyframe *keyframe;
  gsize i;

  for (i = 0; i < self->n_items; i++)
    {
      if (self->items[i].start_time > timestamp)
        break;
    }

  if (i == 0 || i >= self->n_items)
    {
      keyframe = &self->items[i == 0 ? 0 : self->n_items - 1];
#ifdef OTTIE_KEYFRAMES_BY_VALUE
      *out_result = keyframe->start_value;
      return;
#else
      return keyframe->start_value;
#endif
    }

  keyframe = &self->items[i - 1];

  double progress = (timestamp - keyframe->start_time) / (self->items[i].start_time - keyframe->start_time);
#ifdef OTTIE_KEYFRAMES_BY_VALUE
  OTTIE_KEYFRAMES_INTERPOLATE_FUNC (&keyframe->start_value, &keyframe->end_value, progress, out_result);
#else
  return OTTIE_KEYFRAMES_INTERPOLATE_FUNC (keyframe->start_value, keyframe->end_value, progress);
#endif
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

typedef struct
{
  OttieKeyframe keyframe;
  gboolean has_start_value;
  gboolean has_end_value;
} OttieKeyframeParse;

static gboolean
ottie_keyframes(parse_start_value) (JsonReader *reader,
                                    gsize       pos,
                                    gpointer    data)
{
  OttieKeyframeParse *parse = data;

  if (parse->has_start_value)
    ottie_keyframes(free_value) (&parse->keyframe.start_value);

  parse->has_start_value = OTTIE_KEYFRAMES_PARSE_FUNC (reader, 0, &parse->keyframe.start_value);
  return parse->has_start_value;
}

static gboolean
ottie_keyframes(parse_end_value) (JsonReader *reader,
                                  gsize       pos,
                                  gpointer    data)
{
  OttieKeyframeParse *parse = data;

  if (parse->has_end_value)
    ottie_keyframes(free_value) (&parse->keyframe.end_value);

  parse->has_end_value = OTTIE_KEYFRAMES_PARSE_FUNC (reader, 0, &parse->keyframe.end_value);
  return parse->has_end_value;
}

typedef struct
{
  OttieKeyframes *keyframes;
  gboolean has_end_value;
} OttieKeyframesParse;

static gboolean
ottie_keyframes(parse_keyframe) (JsonReader *reader,
                                 gsize       pos,
                                 gpointer    data)
{
  OttieParserOption options[] = {
    { "s", ottie_keyframes(parse_start_value), 0, },
    { "e", ottie_keyframes(parse_end_value), 0, },
    { "t", ottie_parser_option_double, G_STRUCT_OFFSET (OttieKeyframe, start_time) },
    { "i", ottie_keyframes(parse_control_point), G_STRUCT_OFFSET (OttieKeyframe, in) },
    { "o", ottie_keyframes(parse_control_point), G_STRUCT_OFFSET (OttieKeyframe, out) },
    { "ix", ottie_parser_option_skip_index, 0 },
  };
  OttieKeyframesParse *self = data;
  OttieKeyframeParse parse = { 0, };

  if (!ottie_parser_parse_object (reader, "keyframe", options, G_N_ELEMENTS (options), &parse))
    goto fail;

  if (pos == 0)
    {
      if (!parse.has_start_value)
        {
          ottie_parser_error_syntax (reader, "First keyframe must have a start value");
          return FALSE;
        }
    }
  else
    {
      if (parse.keyframe.start_time <= self->keyframes->items[pos - 1].start_time)
        goto fail;

      if (!parse.has_start_value)
        {
          if (self->has_end_value)
            {
              ottie_keyframes(copy_value) (&parse.keyframe.start_value, &self->keyframes->items[pos - 1].end_value);
            }
          else
            {
              ottie_parser_error_syntax (reader, "Keyframe %zu has no end value and %zu has no start value.", pos - 1, pos);
              goto fail;
            }
        }

      if (!self->has_end_value)
        ottie_keyframes(copy_value) (&self->keyframes->items[pos - 1].end_value, &parse.keyframe.start_value);
    }

  self->has_end_value = parse.has_end_value;
  self->keyframes->items[pos] = parse.keyframe;

  return TRUE;

fail:
  self->keyframes->n_items = pos;
  if (parse.has_start_value)
    ottie_keyframes(free_value) (&parse.keyframe.start_value);
  if (parse.has_end_value)
    ottie_keyframes(free_value) (&parse.keyframe.end_value);
  return FALSE;
}

/* no G_GNUC_UNUSED here, if you don't use a type, remove it. */
static inline OttieKeyframes *
ottie_keyframes(parse) (JsonReader *reader)
{
  OttieKeyframesParse parse;
  OttieKeyframes *self;

  self = ottie_keyframes(new) (json_reader_count_elements (reader));

  parse.keyframes = self;
  parse.has_end_value = FALSE;

  if (!ottie_parser_parse_array (reader, "keyframes",
                                 self->n_items, self->n_items,
                                 NULL,
                                 0, 1,
                                 ottie_keyframes(parse_keyframe),
                                 &parse))
    {
      /* do a dumb copy so the free has something to free */
      if (!parse.has_end_value && self->n_items > 0)
        ottie_keyframes(copy_value) (&self->items[self->n_items - 1].end_value,
                                     &self->items[self->n_items - 1].start_value);
      ottie_keyframes(free) (self);
      return NULL;
    }

  if (!parse.has_end_value)
    ottie_keyframes(copy_value) (&self->items[self->n_items - 1].end_value,
                                 &self->items[self->n_items - 1].start_value);

  return parse.keyframes;
}

#ifndef OTTIE_KEYFRAMES_NO_UNDEF

#undef _T_
#undef OttieKeyframes
#undef ottie_keyframes_paste_more
#undef ottie_keyframes_paste
#undef ottie_keyframes

#undef OTTIE_KEYFRAMES_COPY_FUNC
#undef OTTIE_KEYFRAMES_PARSE_FUNC
#undef OTTIE_KEYFRAMES_BY_VALUE
#undef OTTIE_KEYFRAMES_ELEMENT_TYPE
#undef OTTIE_KEYFRAMES_FREE_FUNC
#undef OTTIE_KEYFRAMES_NAME
#undef OTTIE_KEYFRAMES_TYPE_NAME
#endif
