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
#include "gtkaccessiblepropertysetprivate.h"

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

typedef enum {
  GTK_ACCESSIBLE_PROPERTY_CHANGE_ACTIVE_DESCENDANT = 1 << GTK_ACCESSIBLE_PROPERTY_ACTIVE_DESCENDANT,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_AUTOCOMPLETE      = 1 << GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_CONTROLS          = 1 << GTK_ACCESSIBLE_PROPERTY_CONTROLS,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_DESCRIBED_BY      = 1 << GTK_ACCESSIBLE_PROPERTY_DESCRIBED_BY,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_FLOW_TO           = 1 << GTK_ACCESSIBLE_PROPERTY_FLOW_TO,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_HAS_POPUP         = 1 << GTK_ACCESSIBLE_PROPERTY_HAS_POPUP,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_LABEL             = 1 << GTK_ACCESSIBLE_PROPERTY_LABEL,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_LABELLED_BY       = 1 << GTK_ACCESSIBLE_PROPERTY_LABELLED_BY,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_LEVEL             = 1 << GTK_ACCESSIBLE_PROPERTY_LEVEL,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_MULTI_LINE        = 1 << GTK_ACCESSIBLE_PROPERTY_MULTI_LINE,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_MULTI_SELECTABLE  = 1 << GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_ORIENTATION       = 1 << GTK_ACCESSIBLE_PROPERTY_ORIENTATION,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_OWNS              = 1 << GTK_ACCESSIBLE_PROPERTY_OWNS,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_POS_IN_SET        = 1 << GTK_ACCESSIBLE_PROPERTY_POS_IN_SET,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_READ_ONLY         = 1 << GTK_ACCESSIBLE_PROPERTY_READ_ONLY,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_RELEVANT          = 1 << GTK_ACCESSIBLE_PROPERTY_RELEVANT,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_REQUIRED          = 1 << GTK_ACCESSIBLE_PROPERTY_REQUIRED,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_SET_SIZE          = 1 << GTK_ACCESSIBLE_PROPERTY_SET_SIZE,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_SORT              = 1 << GTK_ACCESSIBLE_PROPERTY_SORT,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_VALUE_MAX         = 1 << GTK_ACCESSIBLE_PROPERTY_VALUE_MAX,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_VALUE_MIN         = 1 << GTK_ACCESSIBLE_PROPERTY_VALUE_MIN,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_VALUE_NOW         = 1 << GTK_ACCESSIBLE_PROPERTY_VALUE_NOW,
  GTK_ACCESSIBLE_PROPERTY_CHANGE_VALUE_TEXT        = 1 << GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT
} GtkAccessiblePropertyChange;

struct _GtkATContext
{
  GObject parent_instance;

  GtkAccessibleRole accessible_role;
  GtkAccessible *accessible;

  GtkAccessibleStateSet *states;
  GtkAccessiblePropertySet *properties;
};

struct _GtkATContextClass
{
  GObjectClass parent_class;

  void (* state_change) (GtkATContext                *self,
                         GtkAccessibleStateChange     changed_states,
                         GtkAccessiblePropertyChange  changed_properties,
                         GtkAccessibleStateSet       *states,
                         GtkAccessiblePropertySet    *properties);
};

GtkATContext *  gtk_at_context_create                   (GtkAccessibleRole      accessible_role,
                                                         GtkAccessible         *accessible);

void            gtk_at_context_update                   (GtkATContext          *self);

void            gtk_at_context_set_accessible_state     (GtkATContext          *self,
                                                         GtkAccessibleState     state,
                                                         GtkAccessibleValue    *value);
void            gtk_at_context_set_accessible_property  (GtkATContext          *self,
                                                         GtkAccessibleProperty  property,
                                                         GtkAccessibleValue    *value);

G_END_DECLS
