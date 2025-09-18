/* gtkaccessiblehypertextprivate.h: Private definitions for GtkAccessibleHypertext
 *
 * SPDX-FileCopyrightText: 2025 Red Hat, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gtkaccessiblehypertext.h"

G_BEGIN_DECLS

unsigned int gtk_accessible_hypertext_get_n_links (GtkAccessibleHypertext *self);
GtkAccessibleHyperlink *
             gtk_accessible_hypertext_get_link    (GtkAccessibleHypertext *self,
                                                   unsigned int            index);
unsigned int gtk_accessible_hypertext_get_link_at (GtkAccessibleHypertext *self,
                                                   unsigned int            offset);

unsigned int gtk_accessible_hyperlink_get_index   (GtkAccessibleHyperlink *self);
const char * gtk_accessible_hyperlink_get_uri     (GtkAccessibleHyperlink *self);
void         gtk_accessible_hyperlink_get_extents (GtkAccessibleHyperlink *self,
                                                   GtkAccessibleTextRange *bounds);

G_END_DECLS
