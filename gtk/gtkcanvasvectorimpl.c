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
 * GtkCanvasVector:
 *
 * `GtkCanvasVector` describes a vector in a `GtkCanvas`.
 */

#include "config.h"

#include "gtkcanvasvectorprivate.h"

/* {{{ Boilerplate */

struct _GtkCanvasVectorClass
{
  const char *type_name;

  void                  (* copy)                (GtkCanvasVector        *self,
                                                 const GtkCanvasVector  *source);
  void                  (* finish)              (GtkCanvasVector        *self);
  gboolean              (* eval)                (const GtkCanvasVector  *self,
                                                 graphene_vec2_t        *result);
};

/* }}} */
/* {{{ INVALID */

static void
gtk_canvas_vector_invalid_copy (GtkCanvasVector       *vector,
                                const GtkCanvasVector *source_vector)
{
  gtk_canvas_vector_init_invalid (vector);
}

static void
gtk_canvas_vector_invalid_finish (GtkCanvasVector *vector)
{
}

static gboolean
gtk_canvas_vector_invalid_eval (const GtkCanvasVector *vector,
                                graphene_vec2_t       *result)
{
  return FALSE;
}

static const GtkCanvasVectorClass GTK_CANVAS_VECTOR_INVALID_CLASS =
{
  "GtkCanvasVectorInvalid",
  gtk_canvas_vector_invalid_copy,
  gtk_canvas_vector_invalid_finish,
  gtk_canvas_vector_invalid_eval,
};

void
gtk_canvas_vector_init_invalid (GtkCanvasVector *vector)
{
  vector->class = &GTK_CANVAS_VECTOR_INVALID_CLASS;
}

gboolean
gtk_canvas_vector_is_invalid (GtkCanvasVector *vector)
{
  return vector->class == &GTK_CANVAS_VECTOR_INVALID_CLASS;
}

/* }}} */
/* {{{ CONSTANT */

static void
gtk_canvas_vector_constant_copy (GtkCanvasVector       *vector,
                                 const GtkCanvasVector *source_vector)
{
  const GtkCanvasVectorConstant *source = &source_vector->constant;

  gtk_canvas_vector_init_constant_from_vector (vector, &source->value);
}

static void
gtk_canvas_vector_constant_finish (GtkCanvasVector *vector)
{
}

static gboolean
gtk_canvas_vector_constant_eval (const GtkCanvasVector *vector,
                                 graphene_vec2_t       *result)
{
  const GtkCanvasVectorConstant *self = &vector->constant;

  graphene_vec2_init_from_vec2 (result, &self->value);

  return TRUE;
}

static const GtkCanvasVectorClass GTK_CANVAS_VECTOR_CONSTANT_CLASS =
{
  "GtkCanvasVectorConstant",
  gtk_canvas_vector_constant_copy,
  gtk_canvas_vector_constant_finish,
  gtk_canvas_vector_constant_eval,
};

void
gtk_canvas_vector_init_constant_from_vector (GtkCanvasVector       *vector,
                                             const graphene_vec2_t *value)
{
  GtkCanvasVectorConstant *self = &vector->constant;

  self->class = &GTK_CANVAS_VECTOR_CONSTANT_CLASS;
  graphene_vec2_init_from_vec2 (&self->value, value);
}

void
gtk_canvas_vector_init_constant (GtkCanvasVector *vector,
                               float          x,
                               float          y)
{
  graphene_vec2_t v;

  graphene_vec2_init (&v, x, y);
  gtk_canvas_vector_init_constant_from_vector (vector, &v);
}

/* }}} */
/* {{{ SUM */

static void
gtk_canvas_vector_sum_copy (GtkCanvasVector       *vector,
                            const GtkCanvasVector *source_vector);

static void
gtk_canvas_vector_sum_finish (GtkCanvasVector *vector)
{
  GtkCanvasVectorSum *self = &vector->sum;
  gsize i;

  for (i = 0; i < self->n_summands; i++)
    {
      gtk_canvas_vector_finish (&self->summands[i].value);
    }

  g_free (self->summands);
}

static gboolean
gtk_canvas_vector_sum_eval (const GtkCanvasVector *vector,
                            graphene_vec2_t       *result)
{
  const GtkCanvasVectorSum *self = &vector->sum;
  gsize i;

  if (!gtk_canvas_vector_eval (&self->summands[0].value, result))
    return FALSE;
  graphene_vec2_multiply (&self->summands[0].scale, result, result);

  for (i = 1; i < self->n_summands; i++)
    {
      graphene_vec2_t tmp;

      if (!gtk_canvas_vector_eval (&self->summands[i].value, &tmp))
        return FALSE;
      graphene_vec2_multiply (&self->summands[i].scale, &tmp, &tmp);
      graphene_vec2_add (&tmp, result, result);
    }

  return TRUE;
}

static const GtkCanvasVectorClass GTK_CANVAS_VECTOR_SUM_CLASS =
{
  "GtkCanvasVectorSum",
  gtk_canvas_vector_sum_copy,
  gtk_canvas_vector_sum_finish,
  gtk_canvas_vector_sum_eval,
};

static void
gtk_canvas_vector_sum_copy (GtkCanvasVector       *vector,
                            const GtkCanvasVector *source_vector)
{
  GtkCanvasVectorSum *self = &vector->sum;
  const GtkCanvasVectorSum *source = &source_vector->sum;
  gsize i;

  self->class = &GTK_CANVAS_VECTOR_SUM_CLASS;
  self->summands = g_new (GtkCanvasVectorSummand, source->n_summands);
  self->n_summands = source->n_summands;
  for (i = 0; i < self->n_summands; i++)
    {
      graphene_vec2_init_from_vec2 (&self->summands[i].scale,
                                    &source->summands[i].scale);
      gtk_canvas_vector_init_copy (&self->summands[i].value,
                                   &source->summands[i].value);
    }
}

void
gtk_canvas_vector_init_sum (GtkCanvasVector         *vector,
                            const graphene_vec2_t *scale,
                            ...)
{
  GtkCanvasVectorSum *self = &vector->sum;
  GArray *array;
  GtkCanvasVectorSummand summand;
  va_list args;

  g_assert (scale != NULL);

  self->class = &GTK_CANVAS_VECTOR_SUM_CLASS;

  array = g_array_new (FALSE, FALSE, sizeof (GtkCanvasVectorSummand));
  va_start (args, scale);

  while (scale)
    {
      graphene_vec2_init_from_vec2 (&summand.scale, scale);
      gtk_canvas_vector_init_copy (&summand.value, va_arg (args, const GtkCanvasVector *));
      g_array_append_val (array, summand);
      scale = va_arg (args, const graphene_vec2_t *);
    }
  va_end (args);

  self->n_summands = array->len;
  self->summands = (GtkCanvasVectorSummand *) g_array_free (array, FALSE);
}

/* }}} */
/* {{{ VARIABLE */

static void
gtk_canvas_vector_init_variable_with_variable (GtkCanvasVector *vector,
                                               GtkCanvasVector *variable);

static void
gtk_canvas_vector_variable_copy (GtkCanvasVector       *vector,
                                 const GtkCanvasVector *source_vector)
{
  const GtkCanvasVectorVariable *source = &source_vector->variable;

  gtk_canvas_vector_init_variable_with_variable (vector, g_rc_box_acquire (source->variable));
}

static void
gtk_canvas_vector_variable_finish (GtkCanvasVector *vector)
{
  const GtkCanvasVectorVariable *self = &vector->variable;

  g_rc_box_release (self->variable);
}

static gboolean
gtk_canvas_vector_variable_eval (const GtkCanvasVector *vector,
                                 graphene_vec2_t       *result)
{
  const GtkCanvasVectorVariable *self = &vector->variable;

  return gtk_canvas_vector_eval (self->variable, result);
}

static const GtkCanvasVectorClass GTK_CANVAS_VECTOR_VARIABLE_CLASS =
{
  "GtkCanvasVectorVariable",
  gtk_canvas_vector_variable_copy,
  gtk_canvas_vector_variable_finish,
  gtk_canvas_vector_variable_eval,
};

static void
gtk_canvas_vector_init_variable_with_variable (GtkCanvasVector *vector,
                                               GtkCanvasVector *variable)
{
  GtkCanvasVectorVariable *self = &vector->variable;

  self->class = &GTK_CANVAS_VECTOR_VARIABLE_CLASS;
  
  self->variable = variable;
}

void
gtk_canvas_vector_init_variable (GtkCanvasVector *vector)
{
  GtkCanvasVector *variable;

  variable = g_rc_box_new (GtkCanvasVector);
  gtk_canvas_vector_init_invalid (variable);

  gtk_canvas_vector_init_variable_with_variable (vector, variable);
}

GtkCanvasVector *
gtk_canvas_vector_get_variable (GtkCanvasVector *vector)
{
  GtkCanvasVectorVariable *self = &vector->variable;

  g_return_val_if_fail (self->class == &GTK_CANVAS_VECTOR_VARIABLE_CLASS, NULL);

  return self->variable;
}

/* }}} */
/* {{{ PUBLIC API */

void
gtk_canvas_vector_init_copy (GtkCanvasVector       *self,
                             const GtkCanvasVector *source)
{
  source->class->copy (self, source);
}

void
gtk_canvas_vector_finish (GtkCanvasVector *self)
{
  self->class->finish (self);
}

/**
 * gtk_canvas_vector_eval:
 * @self: a `GtkCanvasVector`
 * @result: (out) (caller-allocates): The current value
 *   of the vector
 *
 * Evaluates the given vector and returns its x and y value.
 *
 * If the vector currently has no value - because it
 * references and object that has been deleted or because
 * the value is in the process of being updated - %FALSE
 * is returned. Think of this as an exception being raised.
 *
 * Returns: %FALSE if the vector currently has no value.
 **/
gboolean
gtk_canvas_vector_eval (const GtkCanvasVector *self,
                        graphene_vec2_t       *result)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  if (self->class->eval (self, result))
    return TRUE;

  graphene_vec2_init_from_vec2 (result, graphene_vec2_zero ());
  return FALSE;
}

