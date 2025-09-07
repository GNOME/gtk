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


#pragma once

#include "gskcomponenttransfer.h"

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcssparserprivate.h"


G_BEGIN_DECLS

typedef enum {
  GSK_COMPONENT_TRANSFER_IDENTITY,
  GSK_COMPONENT_TRANSFER_LEVELS,
  GSK_COMPONENT_TRANSFER_LINEAR,
  GSK_COMPONENT_TRANSFER_GAMMA,
  GSK_COMPONENT_TRANSFER_DISCRETE,
  GSK_COMPONENT_TRANSFER_TABLE,
} GskComponentTransferKind;

struct _GskComponentTransfer
{
  GskComponentTransferKind kind;
  union {
    struct {
      float n;
    } levels;
    struct {
      float m;
      float b;
    } linear;
    struct {
      float amp;
      float exp;
      float ofs;
    } gamma;
    struct {
      guint n;
      float *values;
    } table;
  };
};

static inline void
gsk_component_transfer_init_copy (GskComponentTransfer       *copy,
                                  const GskComponentTransfer *orig)
{
  copy->kind = orig->kind;

  switch (orig->kind)
    {
    case GSK_COMPONENT_TRANSFER_IDENTITY:
      break;
    case GSK_COMPONENT_TRANSFER_LEVELS:
      copy->levels.n = orig->levels.n;
      break;
    case GSK_COMPONENT_TRANSFER_LINEAR:
      copy->linear.m = orig->linear.m;
      copy->linear.b = orig->linear.b;
      break;
    case GSK_COMPONENT_TRANSFER_GAMMA:
      copy->gamma.amp = orig->gamma.amp;
      copy->gamma.exp = orig->gamma.exp;
      copy->gamma.ofs= orig->gamma.ofs;
      break;
    case GSK_COMPONENT_TRANSFER_DISCRETE:
    case GSK_COMPONENT_TRANSFER_TABLE:
      copy->table.n = orig->table.n;
      copy->table.values = g_new (float, copy->table.n);
      memcpy (copy->table.values, orig->table.values, orig->table.n * sizeof (float));
      break;
    default:
      g_assert_not_reached ();
    }
}

static inline void
gsk_component_transfer_clear (GskComponentTransfer *self)
{
  switch (self->kind)
    {
    case GSK_COMPONENT_TRANSFER_IDENTITY:
    case GSK_COMPONENT_TRANSFER_LEVELS:
    case GSK_COMPONENT_TRANSFER_LINEAR:
    case GSK_COMPONENT_TRANSFER_GAMMA:
      break;
    case GSK_COMPONENT_TRANSFER_DISCRETE:
    case GSK_COMPONENT_TRANSFER_TABLE:
      g_free (self->table.values);
      break;
    default:
      g_assert_not_reached ();
    }
}

static inline float
gsk_component_transfer_apply (const GskComponentTransfer *self,
                              float                       c)
{
  switch (self->kind)
    {
    case GSK_COMPONENT_TRANSFER_IDENTITY:
      return c;
    case GSK_COMPONENT_TRANSFER_LEVELS:
      return (floorf (c * self->levels.n) + 0.5f) / self->levels.n;
    case GSK_COMPONENT_TRANSFER_LINEAR:
      return c * self->linear.m + self->linear.b;
    case GSK_COMPONENT_TRANSFER_GAMMA:
      return self->gamma.amp * powf (c, self->gamma.exp) + self->gamma.ofs;
    case GSK_COMPONENT_TRANSFER_DISCRETE:
      {
        float n = self->table.n;
        for (int k = 0; k < n; k++)
          {
            if (k / n <= c && c < (k + 1) / n)
              return self->table.values[k];
          }
        return self->table.values[self->table.n - 1];
      }
    case GSK_COMPONENT_TRANSFER_TABLE:
      {
        float n = self->table.n - 1;
        for (int k = 0; k < n; k++)
          {
            if (k / n <= c && c < (k + 1) / n)
            return self->table.values[k] + (c - k / n) * n * (self->table.values[k+1] - self->table.values[k]);
          }
        return self->table.values[self->table.n - 1];
      }
    default:
      g_assert_not_reached ();
    }
}

void gsk_component_transfer_print (const GskComponentTransfer *self,
                                   GString                    *str);

gboolean gsk_component_transfer_parser_parse (GtkCssParser          *parser,
                                              GskComponentTransfer **out);

G_END_DECLS
