/*
 * Copyright (C) 2023 GNOME Foundation Inc.
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
 * Authors: Alice Mikhaylenko <alicem@gnome.org>
 */

#pragma once

#include "gtkcss.h"
#include "gtkcsstokenizerprivate.h"

G_BEGIN_DECLS

typedef struct _GtkCssVariableValueReference GtkCssVariableValueReference;
typedef struct _GtkCssVariableValue GtkCssVariableValue;

struct _GtkCssVariableValueReference
{
  char *name;
  gsize length;
  GtkCssVariableValue *fallback;
};

struct _GtkCssVariableValue
{
  int ref_count;

  GBytes *bytes;
  gsize offset;
  gsize end_offset;
  gsize length;

  GtkCssVariableValueReference *references;
  gsize n_references;

  GtkCssSection *section;
  gboolean is_invalid;
  gboolean is_animation_tainted;
};

GtkCssVariableValue *gtk_css_variable_value_new         (GBytes                       *bytes,
                                                         gsize                         offset,
                                                         gsize                         end_offset,
                                                         gsize                         length,
                                                         GtkCssVariableValueReference *references,
                                                         gsize                         n_references);
GtkCssVariableValue *gtk_css_variable_value_new_initial (GBytes                       *bytes,
                                                         gsize                         offset,
                                                         gsize                         end_offset);
GtkCssVariableValue *gtk_css_variable_value_ref         (GtkCssVariableValue          *self);
void                 gtk_css_variable_value_unref       (GtkCssVariableValue          *self);
void                 gtk_css_variable_value_print       (GtkCssVariableValue          *self,
                                                         GString                      *string);
char *               gtk_css_variable_value_to_string   (GtkCssVariableValue          *self);
gboolean             gtk_css_variable_value_equal       (const GtkCssVariableValue    *value1,
                                                         const GtkCssVariableValue    *value2) G_GNUC_PURE;
GtkCssVariableValue *gtk_css_variable_value_transition  (GtkCssVariableValue          *start,
                                                         GtkCssVariableValue          *end,
                                                         double                        progress);
void                 gtk_css_variable_value_set_section (GtkCssVariableValue          *self,
                                                         GtkCssSection                *section);
void                 gtk_css_variable_value_taint       (GtkCssVariableValue          *self);

G_END_DECLS
