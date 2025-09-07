/*
 * Copyright © 2025 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gskcomponenttransferprivate.h"

/**
 * GskComponentTransfer:
 *
 * Specifies a transfer function for a color component to be applied
 * while rendering.
 *
 * The available functions include linear, piecewise-linear,
 * gamma and step functions.
 *
 * Note that the transfer function is applied to un-premultiplied
 * values, and all results are clamped to the [0, 1] range.
 *
 * Since: 4.20
 */

G_DEFINE_BOXED_TYPE (GskComponentTransfer, gsk_component_transfer, gsk_component_transfer_copy, gsk_component_transfer_free)

/**
 * gsk_component_transfer_new_identity:
 *
 * Creates a new component transfer that doesn't
 * change the component value.
 *
 * <figure>
 *   <picture>
 *     <source srcset="identity-dark.png" media="(prefers-color-scheme: dark)">
 *       <img alt="Component transfer: identity" src="identity-light.png">
 *   </picture>
 * </figure>
 *
 * Returns: a new `GskComponentTransfer`
 *
 * Since: 4.20
 */
GskComponentTransfer *
gsk_component_transfer_new_identity (void)
{
  GskComponentTransfer *self;

  self = g_new (GskComponentTransfer, 1);

  self->kind = GSK_COMPONENT_TRANSFER_IDENTITY;

  return self;
}

/**
 * gsk_component_transfer_new_levels:
 * @n: Number of levels
 *
 * Creates a new component transfer that limits
 * the values of the component to `n` levels.
 *
 * The new value is computed as
 *
 *     C' = (floor (C * n) + 0.5) / n
 *
 * <figure>
 *   <picture>
 *     <source srcset="levels-dark.png" media="(prefers-color-scheme: dark)">
 *       <img alt="Component transfer: levels" src="levels-light.png">
 *   </picture>
 * </figure>
 *
 * Returns: a new `GskComponentTransfer`
 *
 * Since: 4.20
 */
GskComponentTransfer *
gsk_component_transfer_new_levels (float n)
{
  GskComponentTransfer *self;

  self = g_new (GskComponentTransfer, 1);

  self->kind = GSK_COMPONENT_TRANSFER_LEVELS;
  self->levels.n = n;

  return self;
}

/**
 * gsk_component_transfer_new_linear:
 * @m: Slope
 * @b: Offset
 *
 * Creates a new component transfer that applies
 * a linear transform.
 *
 * The new value is computed as
 *
 *     C' = C * m + b
 *
 * <figure>
 *   <picture>
 *     <source srcset="linear-dark.png" media="(prefers-color-scheme: dark)">
 *       <img alt="Component transfer: linear" src="linear-light.png">
 *   </picture>
 * </figure>
 *
 * Returns: a new `GskComponentTransfer`
 *
 * Since: 4.20
 */
GskComponentTransfer *
gsk_component_transfer_new_linear (float m,
                                   float b)
{
  GskComponentTransfer *self;

  self = g_new (GskComponentTransfer, 1);

  self->kind = GSK_COMPONENT_TRANSFER_LINEAR;
  self->linear.m = m;
  self->linear.b = b;

  return self;
}

/**
 * gsk_component_transfer_new_gamma:
 * @amp: Amplitude
 * @exp: Exponent
 * @ofs: Offset
 *
 * Creates a new component transfer that applies
 * a gamma transform.
 *
 * The new value is computed as
 *
 *     C' = amp * pow (C, exp) + ofs
 *
 * <figure>
 *   <picture>
 *     <source srcset="gamma-dark.png" media="(prefers-color-scheme: dark)">
 *       <img alt="Component transfer: gamma" src="gamma-light.png">
 *   </picture>
 * </figure>
 *
 * Returns: a new `GskComponentTransfer`
 *
 * Since: 4.20
 */
GskComponentTransfer *
gsk_component_transfer_new_gamma (float amp,
                                  float exp,
                                  float ofs)
{
  GskComponentTransfer *self;

  self = g_new (GskComponentTransfer, 1);

  self->kind = GSK_COMPONENT_TRANSFER_GAMMA;
  self->gamma.amp = amp;
  self->gamma.exp = exp;
  self->gamma.ofs = ofs;

  return self;
}

/**
 * gsk_component_transfer_new_discrete:
 * @n: Number of values
 * @values: (array length=n): Values
 *
 * Creates a new component transfer that applies
 * a step function.
 *
 * The new value is computed as
 *
 *     C' = values[k]
 *
 * where k is the smallest value such that
 *
 *     k / n <= C < (k + 1) / n
 *
 * <figure>
 *   <picture>
 *     <source srcset="discrete-dark.png" media="(prefers-color-scheme: dark)">
 *       <img alt="Component transfer: discrete" src="discrete-light.png">
 *   </picture>
 * </figure>
 *
 * Returns: a new `GskComponentTransfer`
 *
 * Since: 4.20
 */
GskComponentTransfer *
gsk_component_transfer_new_discrete (guint  n,
                                     float *values)
{
  GskComponentTransfer *self;

  self = g_new (GskComponentTransfer, 1);

  self->kind = GSK_COMPONENT_TRANSFER_DISCRETE;
  self->table.n = n;
  self->table.values = g_new (float, n);
  memcpy (self->table.values, values, n * sizeof (float));

  return self;
}

/**
 * gsk_component_transfer_new_table:
 * @n: Number of values
 * @values: (array length=n): Values
 *
 * Creates a new component transfer that applies
 * a piecewise linear function.
 *
 * The new value is computed as
 *
 *     C' = values[k] + (C - k / (n - 1)) * n * (values[k + 1] - values[k])
 *
 * where k is the smallest value such that
 *
 *     k / (n - 1) <= C < (k + 1) / (n - 1)
 *
 * <figure>
 *   <picture>
 *     <source srcset="table-dark.png" media="(prefers-color-scheme: dark)">
 *       <img alt="Component transfer: table" src="table-light.png">
 *   </picture>
 * </figure>
 *
 * Returns: a new `GskComponentTransfer`
 *
 * Since: 4.20
 */
GskComponentTransfer *
gsk_component_transfer_new_table (guint  n,
                                  float *values)
{
  GskComponentTransfer *self;

  self = g_new (GskComponentTransfer, 1);

  self->kind = GSK_COMPONENT_TRANSFER_TABLE;
  self->table.n = n;
  self->table.values = g_new (float, n);
  memcpy (self->table.values, values, n * sizeof (float));

  return self;
}

/**
 * gsk_component_transfer_copy:
 * @other: a component transfer
 *
 * Creates a copy of @other.
 *
 * Returns: a newly allocated copy of @other
 *
 * Since: 4.20
 */
GskComponentTransfer *
gsk_component_transfer_copy (const GskComponentTransfer *other)
{
  GskComponentTransfer *self;

  g_return_val_if_fail (other != NULL, NULL);

  self = g_new (GskComponentTransfer, 1);

  gsk_component_transfer_init_copy (self, other);

  return self;
}

/**
 * gsk_component_transfer_free:
 * @self: a component transfer
 *
 * Frees a component transfer.
 *
 * Since: 4.20
 */
void
gsk_component_transfer_free (GskComponentTransfer *self)
{
  if (self == NULL)
    return;

  gsk_component_transfer_clear (self);

  g_free (self);
}

/**
 * gsk_component_transfer_equal:
 * @self: (not nullable): a component transfer
 * @other: (not nullable): another component transfer
 *
 * Compares two component transfers for equality.
 *
 * Returns: true if @self and @other are equal
 *
 * Since: 4.20
 */
gboolean
gsk_component_transfer_equal (gconstpointer self,
                              gconstpointer other)
{
  const GskComponentTransfer *self1 = self;
  const GskComponentTransfer *self2 = other;

  if (self1->kind != self2->kind)
    return FALSE;

  switch (self1->kind)
    {
    case GSK_COMPONENT_TRANSFER_IDENTITY:
      return TRUE;
    case GSK_COMPONENT_TRANSFER_LEVELS:
      return self1->levels.n == self2->levels.n;
    case GSK_COMPONENT_TRANSFER_LINEAR:
      return self1->linear.m == self2->linear.m &&
             self1->linear.b == self2->linear.b;
    case GSK_COMPONENT_TRANSFER_GAMMA:
      return self1->gamma.amp == self2->gamma.amp &&
             self1->gamma.exp == self2->gamma.exp &&
             self1->gamma.ofs == self2->gamma.ofs;
    case GSK_COMPONENT_TRANSFER_DISCRETE:
    case GSK_COMPONENT_TRANSFER_TABLE:
      return self1->table.n == self2->table.n &&
             memcmp (self1->table.values, self2->table.values, self1->table.n * sizeof (float)) == 0;
    default:
      g_assert_not_reached ();
    }
}

void
gsk_component_transfer_print (const GskComponentTransfer *self,
                              GString                    *str)
{
  switch (self->kind)
    {
    case GSK_COMPONENT_TRANSFER_IDENTITY:
      g_string_append (str, "none");
      break;
    case GSK_COMPONENT_TRANSFER_LEVELS:
      g_string_append_printf (str, "levels(%g)", self->levels.n);
      break;
    case GSK_COMPONENT_TRANSFER_LINEAR:
      g_string_append_printf (str, "linear(%g, %g)", self->linear.m, self->linear.b);
      break;
    case GSK_COMPONENT_TRANSFER_GAMMA:
      g_string_append_printf (str, "gamma(%g, %g, %g)", self->gamma.amp, self->gamma.exp, self->gamma.ofs);
      break;
    case GSK_COMPONENT_TRANSFER_DISCRETE:
      g_string_append (str, "discrete(");
      for (guint i = 0; i < self->table.n; i++)
        g_string_append_printf (str, "%s%g", i > 0 ? ", " : "", self->table.values[i]);
      g_string_append (str, ")");
      break;
    case GSK_COMPONENT_TRANSFER_TABLE:
      g_string_append (str, "table(");
      for (guint i = 0; i < self->table.n; i++)
        g_string_append_printf (str, "%s%g", i > 0 ? ", " : "", self->table.values[i]);
      g_string_append (str, ")");
      break;
    default:
      g_assert_not_reached ();
    }
}

static guint
gsk_component_transfer_parse_float (GtkCssParser *parser,
                                    guint         n,
                                    gpointer      data)
{
  float *f = data;
  double d;

  if (!gtk_css_parser_consume_number (parser, &d))
    return 0;

  f[n] = d;
  return 1;
}

static guint
gsk_component_transfer_parse_table_value (GtkCssParser *parser,
                                          guint         n,
                                          gpointer      data)
{
  struct {
    guint n;
    float values[24];
  } *params = data;
  double d;

  if (!gtk_css_parser_consume_number (parser, &d))
    return 0;

  params->values[n] = d;
  params->n++;
  return 1;
}

gboolean
gsk_component_transfer_parser_parse (GtkCssParser          *parser,
                                     GskComponentTransfer **out)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (parser);
  if (gtk_css_token_is_ident (token, "none"))
    {
      gtk_css_parser_consume_token (parser);
      *out = NULL;
      return TRUE;
    }
  else if (gtk_css_token_is_function (token, "levels"))
    {
      float param;

      if (gtk_css_parser_consume_function (parser, 1, 1, gsk_component_transfer_parse_float, &param))
        {
          *out = gsk_component_transfer_new_levels (param);
          return TRUE;
        }
    }
  else if (gtk_css_token_is_function (token, "linear"))
    {
      float params[2];

      if (gtk_css_parser_consume_function (parser, 2, 2, gsk_component_transfer_parse_float, &params))
        {
          *out = gsk_component_transfer_new_linear (params[0], params[1]);
          return TRUE;
        }
    }
  else if (gtk_css_token_is_function (token, "gamma"))
    {
      float params[3];

      if (gtk_css_parser_consume_function (parser, 3, 3, gsk_component_transfer_parse_float, &params))
        {
          *out = gsk_component_transfer_new_gamma (params[0], params[1], params[2]);
          return TRUE;
        }
    }
  else if (gtk_css_token_is_function (token, "discrete"))
    {
      struct {
        guint n;
        float values[24];
      } params = { 0, { 0, } };

      if (gtk_css_parser_consume_function (parser, 1, 24, gsk_component_transfer_parse_table_value, &params))
        {
          *out = gsk_component_transfer_new_discrete (params.n, params.values);
          return TRUE;
        }
    }
  else if (gtk_css_token_is_function (token, "table"))
    {
      struct {
        guint n;
        float values[24];
      } params = { 0, { 0, } };

      if (gtk_css_parser_consume_function (parser, 1, 24, gsk_component_transfer_parse_table_value, &params))
        {
          *out = gsk_component_transfer_new_table (params.n, params.values);
          return TRUE;
        }
    }
  else
    {
      gtk_css_parser_error_syntax (parser, "Expected a component transfer");
    }

  *out = NULL;
  return FALSE;
}
