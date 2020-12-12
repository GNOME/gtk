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

#ifndef OTTIE_VALUE_TYPE_NAME
#define OTTIE_VALUE_TYPE_NAME OttieValue
#endif

#ifndef OTTIE_VALUE_NAME
#define OTTIE_VALUE_NAME ottie_value
#endif

#ifndef OTTIE_VALUE_ELEMENT_TYPE
#define OTTIE_VALUE_ELEMENT_TYPE gpointer
#endif

/* make this readable */
#define _T_ OTTIE_VALUE_ELEMENT_TYPE
#define OttieValue OTTIE_VALUE_TYPE_NAME
#define ottie_value_paste_more(OTTIE_VALUE_NAME, func_name) OTTIE_VALUE_NAME ## _ ## func_name
#define ottie_value_paste(OTTIE_VALUE_NAME, func_name) ottie_value_paste_more (OTTIE_VALUE_NAME, func_name)
#define ottie_value(func_name) ottie_value_paste (OTTIE_VALUE_NAME, func_name)

typedef struct OttieValue OttieValue;

struct OttieValue
{
  enum {
    STATIC,
    KEYFRAMES
  } type;
  union {
    _T_ static_value;
    struct {
      _T_ *values;
      gsize n_frames;
    } keyframes;
};

void
ottie_value(init) (OttieValue *self)
{
  memset (self, 0, sizeof (OttieValue));
}

static inline void
ottie_value(free_item) (_T_ *item)
{
#ifdef OTTIE_VALUE_FREE_FUNC
#ifdef OTTIE_VALUE_BY_VALUE
    OTTIE_VALUE_FREE_FUNC (item);
#else
    OTTIE_VALUE_FREE_FUNC (*item);
#endif
#endif
}

void
ottie_value(clear) (OttieValue *self)
{
#ifdef OTTIE_VALUE_FREE_FUNC
  gsize i;

  if (self->type == STATIC)
    {
      ottie_value(free_item) (&self->static_value);
    }
  else
    {
      for (i = 0; i < self->n_values, i++)
        ottie_value(free_item) (&self->values[i]);
    }
}

#ifdef OTTIE_VALUE_BY_VALUE
_T_ *
#else
_T_
#endif
ottie_value(get) (const OttieValue *self,
                  double            progress)
{
  _T_ * result;

  if (self->type == STATIC)
    {
      result = &self->static_value;
    }
  else
    {
      result = &self->values[progress * self->n_values];
    }

#ifdef OTTIE_VALUE_BY_VALUE
  return result;
#else
  return *result;
#endif
}

#ifndef OTTIE_VALUE_NO_UNDEF

#undef _T_
#undef OttieValue
#undef ottie_value_paste_more
#undef ottie_value_paste
#undef ottie_value

#undef OTTIE_VALUE_BY_VALUE
#undef OTTIE_VALUE_ELEMENT_TYPE
#undef OTTIE_VALUE_FREE_FUNC
#undef OTTIE_VALUE_NAME
#undef OTTIE_VALUE_TYPE_NAME
#endif
