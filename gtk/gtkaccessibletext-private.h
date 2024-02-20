/* gtkaccessibletext-private.h: Private definitions for GtkAccessibleText
 *
 * SPDX-FileCopyrightText: 2023 Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gtkaccessibletext.h"

G_BEGIN_DECLS

GBytes *
gtk_accessible_text_get_contents (GtkAccessibleText *self,
                                  unsigned int       start,
                                  unsigned int       end);

GBytes *
gtk_accessible_text_get_contents_at (GtkAccessibleText            *self,
                                     unsigned int                  offset,
                                     GtkAccessibleTextGranularity  granularity,
                                     unsigned int                 *start,
                                     unsigned int                 *end);

unsigned int
gtk_accessible_text_get_caret_position (GtkAccessibleText *self);

gboolean
gtk_accessible_text_get_selection (GtkAccessibleText       *self,
                                   gsize                   *n_ranges,
                                   GtkAccessibleTextRange **ranges);

gboolean
gtk_accessible_text_get_attributes (GtkAccessibleText        *self,
                                    unsigned int              offset,
                                    gsize                    *n_ranges,
                                    GtkAccessibleTextRange  **ranges,
                                    char                   ***attribute_names,
                                    char                   ***attribute_values);
G_END_DECLS
