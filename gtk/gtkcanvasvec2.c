/*
 * Copyright © 2022 Benjamin Otte
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


/**
 * GtkCanvasVec2:
 *
 * `GtkCanvasVec2` describes a vec2 in a `GtkCanvas`.
 */

#include "config.h"

#include "gtkcanvasvec2private.h"

/* {{{ Boilerplate */

struct _GtkCanvasVec2Class
{
  const char *type_name;

  void                  (* copy)                (GtkCanvasVec2          *self,
                                                 const GtkCanvasVec2    *source);
  void                  (* finish)              (GtkCanvasVec2          *self);
  gboolean              (* eval)                (const GtkCanvasVec2    *self,
                                                 graphene_vec2_t        *result);
};

/* }}} */
/* {{{ INVALID */

static void
gtk_canvas_vec2_invalid_copy (GtkCanvasVec2       *vec2,
                              const GtkCanvasVec2 *source_vec2)
{
  gtk_canvas_vec2_init_invalid (vec2);
}

static void
gtk_canvas_vec2_invalid_finish (GtkCanvasVec2 *vec2)
{
}

static gboolean
gtk_canvas_vec2_invalid_eval (const GtkCanvasVec2 *vec2,
                              graphene_vec2_t     *result)
{
  return FALSE;
}

static const GtkCanvasVec2Class GTK_CANVAS_VEC2_INVALID_CLASS =
{
  "GtkCanvasVec2Invalid",
  gtk_canvas_vec2_invalid_copy,
  gtk_canvas_vec2_invalid_finish,
  gtk_canvas_vec2_invalid_eval,
};

void
gtk_canvas_vec2_init_invalid (GtkCanvasVec2 *vec2)
{
  vec2->class = &GTK_CANVAS_VEC2_INVALID_CLASS;
}

/* }}} */
/* {{{ CONSTANT */

static void
gtk_canvas_vec2_constant_copy (GtkCanvasVec2       *vec2,
                               const GtkCanvasVec2 *source_vec2)
{
  const GtkCanvasVec2Constant *source = &source_vec2->constant;

  gtk_canvas_vec2_init_constant_from_vec2 (vec2, &source->value);
}

static void
gtk_canvas_vec2_constant_finish (GtkCanvasVec2 *vec2)
{
}

static gboolean
gtk_canvas_vec2_constant_eval (const GtkCanvasVec2 *vec2,
                               graphene_vec2_t     *result)
{
  const GtkCanvasVec2Constant *self = &vec2->constant;

  graphene_vec2_init_from_vec2 (result, &self->value);

  return TRUE;
}

static const GtkCanvasVec2Class GTK_CANVAS_VEC2_CONSTANT_CLASS =
{
  "GtkCanvasVec2Constant",
  gtk_canvas_vec2_constant_copy,
  gtk_canvas_vec2_constant_finish,
  gtk_canvas_vec2_constant_eval,
};

void
gtk_canvas_vec2_init_constant_from_vec2 (GtkCanvasVec2         *vec2,
                                         const graphene_vec2_t *value)
{
  GtkCanvasVec2Constant *self = &vec2->constant;

  self->class = &GTK_CANVAS_VEC2_CONSTANT_CLASS;
  graphene_vec2_init_from_vec2 (&self->value, value);
}

void
gtk_canvas_vec2_init_constant (GtkCanvasVec2 *vec2,
                               float          x,
                               float          y)
{
  graphene_vec2_t v;

  graphene_vec2_init (&v, x, y);
  gtk_canvas_vec2_init_constant_from_vec2 (vec2, &v);
}

/* }}} */
/* {{{ SUM */

static void
gtk_canvas_vec2_sum_copy (GtkCanvasVec2       *vec2,
                          const GtkCanvasVec2 *source_vec2);

static void
gtk_canvas_vec2_sum_finish (GtkCanvasVec2 *vec2)
{
  GtkCanvasVec2Sum *self = &vec2->sum;
  gsize i;

  for (i = 0; i < self->n_summands; i++)
    {
      gtk_canvas_vec2_finish (&self->summands[i].value);
    }

  g_free (self->summands);
}

static gboolean
gtk_canvas_vec2_sum_eval (const GtkCanvasVec2 *vec2,
                          graphene_vec2_t     *result)
{
  const GtkCanvasVec2Sum *self = &vec2->sum;
  gsize i;

  if (!gtk_canvas_vec2_eval (&self->summands[0].value, result))
    return FALSE;
  graphene_vec2_multiply (&self->summands[0].scale, result, result);

  for (i = 1; i < self->n_summands; i++)
    {
      graphene_vec2_t tmp;

      if (!gtk_canvas_vec2_eval (&self->summands[i].value, &tmp))
        return FALSE;
      graphene_vec2_multiply (&self->summands[i].scale, &tmp, &tmp);
      graphene_vec2_add (&tmp, result, result);
    }

  return TRUE;
}

static const GtkCanvasVec2Class GTK_CANVAS_VEC2_SUM_CLASS =
{
  "GtkCanvasVec2Sum",
  gtk_canvas_vec2_sum_copy,
  gtk_canvas_vec2_sum_finish,
  gtk_canvas_vec2_sum_eval,
};

static void
gtk_canvas_vec2_sum_copy (GtkCanvasVec2       *vec2,
                          const GtkCanvasVec2 *source_vec2)
{
  GtkCanvasVec2Sum *self = &vec2->sum;
  const GtkCanvasVec2Sum *source = &source_vec2->sum;
  gsize i;

  self->class = &GTK_CANVAS_VEC2_SUM_CLASS;
  self->summands = g_new (GtkCanvasVec2Summand, source->n_summands);
  self->n_summands = source->n_summands;
  for (i = 0; i < self->n_summands; i++)
    {
      graphene_vec2_init_from_vec2 (&self->summands[i].scale,
                                    &source->summands[i].scale);
      gtk_canvas_vec2_init_copy (&self->summands[i].value,
                                 &source->summands[i].value);
    }

}

void
gtk_canvas_vec2_init_sum (GtkCanvasVec2         *vec2,
                          const graphene_vec2_t *scale,
                          ...)
{
  GtkCanvasVec2Sum *self = &vec2->sum;
  GArray *array;
  GtkCanvasVec2Summand summand;
  va_list args;

  g_assert (scale != NULL);

  self->class = &GTK_CANVAS_VEC2_SUM_CLASS;

  array = g_array_new (FALSE, FALSE, sizeof (GtkCanvasVec2Summand));
  va_start (args, scale);

  while (scale)
    {
      graphene_vec2_init_from_vec2 (&summand.scale, scale);
      gtk_canvas_vec2_init_copy (&summand.value, va_arg (args, const GtkCanvasVec2 *));
      g_array_append_val (array, summand);
      scale = va_arg (args, const graphene_vec2_t *);
    }
  va_end (args);

  self->n_summands = array->len;
  self->summands = (GtkCanvasVec2Summand *) g_array_free (array, FALSE);
}

/* }}} */
/* {{{ VARIABLE */

static void
gtk_canvas_vec2_init_variable_with_variable (GtkCanvasVec2 *vec2,
                                             GtkCanvasVec2 *variable);

static void
gtk_canvas_vec2_variable_copy (GtkCanvasVec2       *vec2,
                               const GtkCanvasVec2 *source_vec2)
{
  const GtkCanvasVec2Variable *source = &source_vec2->variable;

  gtk_canvas_vec2_init_variable_with_variable (vec2, g_rc_box_acquire (source->variable));
}

static void
gtk_canvas_vec2_variable_finish (GtkCanvasVec2 *vec2)
{
  const GtkCanvasVec2Variable *self = &vec2->variable;

  g_rc_box_release (self->variable);
}

static gboolean
gtk_canvas_vec2_variable_eval (const GtkCanvasVec2 *vec2,
                               graphene_vec2_t     *result)
{
  const GtkCanvasVec2Variable *self = &vec2->variable;

  return gtk_canvas_vec2_eval (self->variable, result);
}

static const GtkCanvasVec2Class GTK_CANVAS_VEC2_VARIABLE_CLASS =
{
  "GtkCanvasVec2Variable",
  gtk_canvas_vec2_variable_copy,
  gtk_canvas_vec2_variable_finish,
  gtk_canvas_vec2_variable_eval,
};

static void
gtk_canvas_vec2_init_variable_with_variable (GtkCanvasVec2 *vec2,
                                             GtkCanvasVec2 *variable)
{
  GtkCanvasVec2Variable *self = &vec2->variable;

  self->class = &GTK_CANVAS_VEC2_VARIABLE_CLASS;
  
  self->variable = variable;
}

void
gtk_canvas_vec2_init_variable (GtkCanvasVec2 *vec2)
{
  GtkCanvasVec2 *variable;

  variable = g_rc_box_new (GtkCanvasVec2);
  gtk_canvas_vec2_init_invalid (variable);

  gtk_canvas_vec2_init_variable_with_variable (vec2, variable);
}

GtkCanvasVec2 *
gtk_canvas_vec2_get_variable (GtkCanvasVec2 *vec2)
{
  GtkCanvasVec2Variable *self = &vec2->variable;

  g_return_val_if_fail (self->class == &GTK_CANVAS_VEC2_VARIABLE_CLASS, NULL);

  return self->variable;
}

/* }}} */
/* {{{ PUBLIC API */

void
gtk_canvas_vec2_init_copy (GtkCanvasVec2       *self,
                           const GtkCanvasVec2 *source)
{
  source->class->copy (self, source);
}

void
gtk_canvas_vec2_finish (GtkCanvasVec2 *self)
{
  self->class->finish (self);
}

gboolean
gtk_canvas_vec2_eval (const GtkCanvasVec2 *self,
                      graphene_vec2_t     *result)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  if (self->class->eval (self, result))
    return TRUE;

  graphene_vec2_init_from_vec2 (result, graphene_vec2_zero ());
  return FALSE;
}

