/* gtkaccessiblevalueprivate.h: Accessible value
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>

#include "gtkaccessible.h"
#include "gtkenums.h"

G_BEGIN_DECLS

#define GTK_TYPE_ACCESSIBLE_VALUE (gtk_accessible_value_get_type())

#define GTK_ACCESSIBLE_VALUE_ERROR (gtk_accessible_value_error_quark())

typedef struct _GtkAccessibleValue      GtkAccessibleValue;
typedef struct _GtkAccessibleValueClass GtkAccessibleValueClass;

struct _GtkAccessibleValue
{
  const GtkAccessibleValueClass *value_class;

  int ref_count;
};

struct _GtkAccessibleValueClass
{
  const char *type_name;
  gsize instance_size;

  void (* init) (GtkAccessibleValue *self);
  void (* finalize) (GtkAccessibleValue *self);
  void (* print) (const GtkAccessibleValue *self,
                  GString *string);
  gboolean (* equal) (const GtkAccessibleValue *value_a,
                      const GtkAccessibleValue *value_b);
};

typedef enum {
  GTK_ACCESSIBLE_VALUE_ERROR_READ_ONLY,
  GTK_ACCESSIBLE_VALUE_ERROR_INVALID_VALUE,
  GTK_ACCESSIBLE_VALUE_ERROR_INVALID_RANGE,
  GTK_ACCESSIBLE_VALUE_ERROR_INVALID_TOKEN
} GtkAccessibleValueError;

GType                   gtk_accessible_value_get_type                   (void) G_GNUC_CONST;
GQuark                  gtk_accessible_value_error_quark                (void);

GtkAccessibleValue *    gtk_accessible_value_alloc                      (const GtkAccessibleValueClass *klass);
GtkAccessibleValue *    gtk_accessible_value_ref                        (GtkAccessibleValue            *self);
void                    gtk_accessible_value_unref                      (GtkAccessibleValue            *self);
void                    gtk_accessible_value_print                      (const GtkAccessibleValue      *self,
                                                                         GString                       *buffer);
char *                  gtk_accessible_value_to_string                  (const GtkAccessibleValue      *self);
gboolean                gtk_accessible_value_equal                      (const GtkAccessibleValue      *value_a,
                                                                         const GtkAccessibleValue      *value_b);

GtkAccessibleValue *    gtk_accessible_value_get_default_for_state      (GtkAccessibleState             state);
GtkAccessibleValue *    gtk_accessible_value_collect_for_state          (GtkAccessibleState             state,
                                                                         va_list                       *args);
GtkAccessibleValue *    gtk_accessible_value_collect_for_state_value    (GtkAccessibleState             state,
                                                                         const GValue                  *value);

GtkAccessibleValue *    gtk_accessible_value_get_default_for_property   (GtkAccessibleProperty          property);
GtkAccessibleValue *    gtk_accessible_value_collect_for_property       (GtkAccessibleProperty          property,
                                                                         va_list                       *args);
GtkAccessibleValue *    gtk_accessible_value_collect_for_property_value (GtkAccessibleProperty          property,
                                                                         const GValue                  *value);

/* Basic values */
GtkAccessibleValue *            gtk_undefined_accessible_value_new      (void);

GtkAccessibleValue *            gtk_boolean_accessible_value_new        (gboolean                  value);
gboolean                        gtk_boolean_accessible_value_get        (const GtkAccessibleValue *value);

GtkAccessibleValue *            gtk_int_accessible_value_new            (int                       value);
int                             gtk_int_accessible_value_get            (const GtkAccessibleValue *value);

GtkAccessibleValue *            gtk_number_accessible_value_new         (double                    value);
double                          gtk_number_accessible_value_get         (const GtkAccessibleValue *value);

GtkAccessibleValue *            gtk_string_accessible_value_new         (const char               *value);
const char *                    gtk_string_accessible_value_get         (const GtkAccessibleValue *value);

GtkAccessibleValue *            gtk_reference_accessible_value_new      (GtkAccessible            *value);
GtkAccessible *                 gtk_reference_accessible_value_get      (const GtkAccessibleValue *value);

/* Tri-state values */
GtkAccessibleValue *            gtk_expanded_accessible_value_new       (int                       value);
int                             gtk_expanded_accessible_value_get       (const GtkAccessibleValue *value);

GtkAccessibleValue *            gtk_grabbed_accessible_value_new        (int                       value);
int                             gtk_grabbed_accessible_value_get        (const GtkAccessibleValue *value);

GtkAccessibleValue *            gtk_selected_accessible_value_new       (int                       value);
int                             gtk_selected_accessible_value_get       (const GtkAccessibleValue *value);

/* Token values */
GtkAccessibleValue *            gtk_pressed_accessible_value_new        (GtkAccessiblePressedState value);
GtkAccessiblePressedState       gtk_pressed_accessible_value_get        (const GtkAccessibleValue *value);

GtkAccessibleValue *            gtk_checked_accessible_value_new        (GtkAccessibleCheckedState value);
GtkAccessibleCheckedState       gtk_checked_accessible_value_get        (const GtkAccessibleValue *value);

GtkAccessibleValue *            gtk_invalid_accessible_value_new        (GtkAccessibleInvalidState value);
GtkAccessibleInvalidState       gtk_invalid_accessible_value_get        (const GtkAccessibleValue *value);

GtkAccessibleValue *            gtk_autocomplete_accessible_value_new   (GtkAccessibleAutocomplete value);
GtkAccessibleAutocomplete       gtk_autocomplete_accessible_value_get   (const GtkAccessibleValue *value);

GtkAccessibleValue *            gtk_orientation_accessible_value_new    (GtkOrientation            value);
GtkOrientation                  gtk_orientation_accessible_value_get    (const GtkAccessibleValue *value);

GtkAccessibleValue *            gtk_sort_accessible_value_new           (GtkAccessibleSort         value);
GtkAccessibleSort               gtk_sort_accessible_value_get           (const GtkAccessibleValue *value);

G_END_DECLS
