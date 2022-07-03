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

#ifndef __GTK_CANVAS_VEC2_PRIVATE_H__
#define __GTK_CANVAS_VEC2_PRIVATE_H__

#include <glib.h>
#include <graphene.h>

G_BEGIN_DECLS

typedef struct _GtkCanvasVec2 GtkCanvasVec2;
typedef struct _GtkCanvasVec2Class GtkCanvasVec2Class;
typedef struct _GtkCanvasVec2Constant GtkCanvasVec2Constant;
typedef struct _GtkCanvasVec2Sum GtkCanvasVec2Sum;
typedef struct _GtkCanvasVec2Summand GtkCanvasVec2Summand;
typedef struct _GtkCanvasVec2Variable GtkCanvasVec2Variable;

struct _GtkCanvasVec2Constant
{
  const GtkCanvasVec2Class *class;

  graphene_vec2_t value;
};

struct _GtkCanvasVec2Sum
{
  const GtkCanvasVec2Class *class;
  
  graphene_vec2_t constant;

  gsize n_summands;
  GtkCanvasVec2Summand *summands;
};

struct _GtkCanvasVec2Variable
{
  const GtkCanvasVec2Class *class;

  /* a GtkRcBox */
  GtkCanvasVec2 *variable;
};

struct _GtkCanvasVec2
{
  union {
    const GtkCanvasVec2Class *class;
    GtkCanvasVec2Constant constant;
    GtkCanvasVec2Sum sum;
    GtkCanvasVec2Variable variable;
  };
};

struct _GtkCanvasVec2Summand
{
  graphene_vec2_t scale;
  GtkCanvasVec2 value;
};


void                    gtk_canvas_vec2_init_copy               (GtkCanvasVec2          *self,
                                                                 const GtkCanvasVec2    *source);
void                    gtk_canvas_vec2_finish                  (GtkCanvasVec2          *self);

gboolean                gtk_canvas_vec2_eval                    (const GtkCanvasVec2    *self,
                                                                 graphene_vec2_t        *result);

void                    gtk_canvas_vec2_init_invalid            (GtkCanvasVec2          *vec2);
void                    gtk_canvas_vec2_init_constant           (GtkCanvasVec2          *vec2,
                                                                 float                   x,
                                                                 float                   y);
void                    gtk_canvas_vec2_init_constant_from_vec2 (GtkCanvasVec2          *vec2,
                                                                 const graphene_vec2_t  *value);
void                    gtk_canvas_vec2_init_sum                (GtkCanvasVec2          *vec2,
                                                                 const graphene_vec2_t  *scale,
                                                                 ...) G_GNUC_NULL_TERMINATED;

void                    gtk_canvas_vec2_init_variable           (GtkCanvasVec2          *vec2);
GtkCanvasVec2 *         gtk_canvas_vec2_get_variable            (GtkCanvasVec2          *vec2);

gboolean                gtk_canvas_vec2_is_invalid              (GtkCanvasVec2          *vec2);

G_END_DECLS

#endif /* __GTK_CANVAS_VEC2_PRIVATE_H__ */
