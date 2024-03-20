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

#include "gtk/css/gtkcssvariablevalueprivate.h"

G_BEGIN_DECLS

typedef struct _GtkCssVariableSet GtkCssVariableSet;

struct _GtkCssVariableSet
{
  int ref_count;

  GHashTable *variables;
  GtkCssVariableSet *parent;
};

GtkCssVariableSet *  gtk_css_variable_set_new            (void);

GtkCssVariableSet *  gtk_css_variable_set_ref            (GtkCssVariableSet   *self);
void                 gtk_css_variable_set_unref          (GtkCssVariableSet   *self);

GtkCssVariableSet *  gtk_css_variable_set_copy           (GtkCssVariableSet   *self);

void                 gtk_css_variable_set_set_parent     (GtkCssVariableSet   *self,
                                                          GtkCssVariableSet   *parent);

void                 gtk_css_variable_set_add            (GtkCssVariableSet   *self,
                                                          int                  id,
                                                          GtkCssVariableValue *value);
void                 gtk_css_variable_set_resolve_cycles (GtkCssVariableSet   *self);

GtkCssVariableValue *gtk_css_variable_set_lookup         (GtkCssVariableSet   *self,
                                                          int                  id,
                                                          GtkCssVariableSet  **source);

GArray *             gtk_css_variable_set_list_ids       (GtkCssVariableSet   *self);

gboolean             gtk_css_variable_set_equal          (GtkCssVariableSet   *set1,
                                                          GtkCssVariableSet   *set2);

G_END_DECLS
