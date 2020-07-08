/* gtkatcontextprivate.h: Private header for GtkATContext
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

#include "gtkatcontext.h"

#include "gtkaccessiblestatesetprivate.h"

G_BEGIN_DECLS

typedef enum {
 GTK_ACCESSIBLE_STATE_CHANGE_BUSY     = 1 << GTK_ACCESSIBLE_STATE_BUSY,
 GTK_ACCESSIBLE_STATE_CHANGE_CHECKED  = 1 << GTK_ACCESSIBLE_STATE_CHECKED,
 GTK_ACCESSIBLE_STATE_CHANGE_DISABLED = 1 << GTK_ACCESSIBLE_STATE_DISABLED,
 GTK_ACCESSIBLE_STATE_CHANGE_EXPANDED = 1 << GTK_ACCESSIBLE_STATE_EXPANDED,
 GTK_ACCESSIBLE_STATE_CHANGE_GRABBED  = 1 << GTK_ACCESSIBLE_STATE_GRABBED,
 GTK_ACCESSIBLE_STATE_CHANGE_HIDDEN   = 1 << GTK_ACCESSIBLE_STATE_HIDDEN,
 GTK_ACCESSIBLE_STATE_CHANGE_INVALID  = 1 << GTK_ACCESSIBLE_STATE_INVALID,
 GTK_ACCESSIBLE_STATE_CHANGE_PRESSED  = 1 << GTK_ACCESSIBLE_STATE_PRESSED,
 GTK_ACCESSIBLE_STATE_CHANGE_SELECTED = 1 << GTK_ACCESSIBLE_STATE_SELECTED
} GtkAccessibleStateChange;

struct _GtkATContext
{
  GObject parent_instance;

  GtkAccessibleRole accessible_role;
  GtkAccessible *accessible;

  GtkAccessibleStateSet *states;
};

struct _GtkATContextClass
{
  GObjectClass parent_class;

  void (* state_change) (GtkATContext             *self,
                         GtkAccessibleStateChange  changed,
                         GtkAccessibleStateSet    *states);
};

GtkATContext *  gtk_at_context_create           (GtkAccessibleRole   accessible_role,
                                                 GtkAccessible      *accessible);

void            gtk_at_context_update_state     (GtkATContext       *self);

void            gtk_at_context_set_state        (GtkATContext       *self,
                                                 GtkAccessibleState  state,
                                                 GtkAccessibleValue *value);

G_END_DECLS
