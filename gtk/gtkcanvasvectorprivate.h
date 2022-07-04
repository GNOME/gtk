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

#ifndef __GTK_CANVAS_VECTOR_PRIVATE_H__
#define __GTK_CANVAS_VECTOR_PRIVATE_H__

#include "gtkcanvasvector.h"

#include <graphene.h>

G_BEGIN_DECLS

typedef struct _GtkCanvasVector GtkCanvasVector;
typedef struct _GtkCanvasVectorClass GtkCanvasVectorClass;
typedef struct _GtkCanvasVectorConstant GtkCanvasVectorConstant;
typedef struct _GtkCanvasVectorSum GtkCanvasVectorSum;
typedef struct _GtkCanvasVectorSummand GtkCanvasVectorSummand;
typedef struct _GtkCanvasVectorVariable GtkCanvasVectorVariable;

struct _GtkCanvasVectorConstant
{
  const GtkCanvasVectorClass *class;

  graphene_vec2_t value;
};

struct _GtkCanvasVectorSum
{
  const GtkCanvasVectorClass *class;
  
  graphene_vec2_t constant;

  gsize n_summands;
  GtkCanvasVectorSummand *summands;
};

struct _GtkCanvasVectorVariable
{
  const GtkCanvasVectorClass *class;

  /* a GtkRcBox */
  GtkCanvasVector *variable;
};

struct _GtkCanvasVector
{
  union {
    const GtkCanvasVectorClass *class;
    GtkCanvasVectorConstant constant;
    GtkCanvasVectorSum sum;
    GtkCanvasVectorVariable variable;
  };
};

struct _GtkCanvasVectorSummand
{
  graphene_vec2_t scale;
  GtkCanvasVector value;
};


void                    gtk_canvas_vector_init_copy             (GtkCanvasVector        *self,
                                                                 const GtkCanvasVector  *source);
void                    gtk_canvas_vector_finish                (GtkCanvasVector        *self);

void                    gtk_canvas_vector_init_invalid          (GtkCanvasVector        *vector);
void                    gtk_canvas_vector_init_constant         (GtkCanvasVector        *vector,
                                                                 float                   x,
                                                                 float                   y);
void                    gtk_canvas_vector_init_constant_from_vector (GtkCanvasVector    *vector,
                                                                 const graphene_vec2_t  *value);
void                    gtk_canvas_vector_init_sum              (GtkCanvasVector        *vector,
                                                                 const graphene_vec2_t  *scale,
                                                                 ...) G_GNUC_NULL_TERMINATED;

void                    gtk_canvas_vector_init_variable         (GtkCanvasVector        *vector);
GtkCanvasVector *       gtk_canvas_vector_get_variable          (GtkCanvasVector        *vector);

gboolean                gtk_canvas_vector_is_invalid            (GtkCanvasVector        *vector);

G_END_DECLS

#endif /* __GTK_CANVAS_VECTOR_PRIVATE_H__ */
