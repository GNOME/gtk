/* gtkaccessiblerelationset.c: Accessible relation set
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

#include "config.h"

#include "gtkaccessiblerelationsetprivate.h"

#include "gtkbitmaskprivate.h"
#include "gtkenums.h"

/* Keep in sync with GtkAccessibleRelation in gtkenums.h */
#define LAST_RELATION GTK_ACCESSIBLE_RELATION_SET_SIZE

struct _GtkAccessibleRelationSet
{
  GtkBitmask *relation_set;

  GtkAccessibleValue **relation_values;
};

static GtkAccessibleRelationSet *
gtk_accessible_relation_set_init (GtkAccessibleRelationSet *self)
{
  self->relation_set = _gtk_bitmask_new ();
  self->relation_values = g_new (GtkAccessibleValue *, LAST_RELATION);

  /* Initialize all relation values, so we can always get the full set */
  for (int i = 0; i < LAST_RELATION; i++)
    self->relation_values[i] = gtk_accessible_value_get_default_for_relation (i);

  return self;
}

GtkAccessibleRelationSet *
gtk_accessible_relation_set_new (void)
{
  GtkAccessibleRelationSet *set = g_rc_box_new0 (GtkAccessibleRelationSet);

  return gtk_accessible_relation_set_init (set);
}

GtkAccessibleRelationSet *
gtk_accessible_relation_set_ref (GtkAccessibleRelationSet *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return g_rc_box_acquire (self);
}

static void
gtk_accessible_relation_set_free (gpointer data)
{
  GtkAccessibleRelationSet *self = data;

  for (int i = 0; i < LAST_RELATION; i++)
    {
      if (self->relation_values[i] != NULL)
        gtk_accessible_value_unref (self->relation_values[i]);
    }

  g_free (self->relation_values);

  _gtk_bitmask_free (self->relation_set);
}

void
gtk_accessible_relation_set_unref (GtkAccessibleRelationSet *self)
{
  g_rc_box_release_full (self, gtk_accessible_relation_set_free);
}

void
gtk_accessible_relation_set_add (GtkAccessibleRelationSet *self,
                                 GtkAccessibleRelation     relation,
                                 GtkAccessibleValue       *value)
{
  g_return_if_fail (relation >= GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT &&
                    relation <= GTK_ACCESSIBLE_RELATION_SET_SIZE);

  if (gtk_accessible_relation_set_contains (self, relation))
    gtk_accessible_value_unref (self->relation_values[relation]);
  else
    self->relation_set = _gtk_bitmask_set (self->relation_set, relation, TRUE);

  self->relation_values[relation] = gtk_accessible_value_ref (value);
}

void
gtk_accessible_relation_set_remove (GtkAccessibleRelationSet *self,
                                    GtkAccessibleRelation     relation)
{
  g_return_if_fail (relation >= GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT &&
                    relation <= GTK_ACCESSIBLE_RELATION_SET_SIZE);

  if (gtk_accessible_relation_set_contains (self, relation))
    {
      g_clear_pointer (&(self->relation_values[relation]), gtk_accessible_value_unref);
      self->relation_set = _gtk_bitmask_set (self->relation_set, relation, FALSE);
    }
}

gboolean
gtk_accessible_relation_set_contains (GtkAccessibleRelationSet *self,
                                      GtkAccessibleRelation     relation)
{
  g_return_val_if_fail (relation >= GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT &&
                        relation <= GTK_ACCESSIBLE_RELATION_SET_SIZE,
                        FALSE);

  return _gtk_bitmask_get (self->relation_set, relation);
}

GtkAccessibleValue *
gtk_accessible_relation_set_get_value (GtkAccessibleRelationSet *self,
                                       GtkAccessibleRelation     relation)
{
  g_return_val_if_fail (relation >= GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT &&
                        relation <= GTK_ACCESSIBLE_RELATION_SET_SIZE,
                        NULL);

  return self->relation_values[relation];
}

static const char *relation_names[] = {
  [GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT] = "activedescendant",
  [GTK_ACCESSIBLE_RELATION_COL_COUNT] = "colcount",
  [GTK_ACCESSIBLE_RELATION_COL_INDEX] = "colindex",
  [GTK_ACCESSIBLE_RELATION_COL_INDEX_TEXT] = "colindextext",
  [GTK_ACCESSIBLE_RELATION_COL_SPAN] = "colspan",
  [GTK_ACCESSIBLE_RELATION_CONTROLS] = "controls",
  [GTK_ACCESSIBLE_RELATION_DESCRIBED_BY] = "describedby",
  [GTK_ACCESSIBLE_RELATION_DETAILS] = "details",
  [GTK_ACCESSIBLE_RELATION_ERROR_MESSAGE] = "errormessage",
  [GTK_ACCESSIBLE_RELATION_FLOW_TO] = "flowto",
  [GTK_ACCESSIBLE_RELATION_LABELLED_BY] = "labelledby",
  [GTK_ACCESSIBLE_RELATION_OWNS] = "owns",
  [GTK_ACCESSIBLE_RELATION_POS_IN_SET] = "posinset",
  [GTK_ACCESSIBLE_RELATION_ROW_COUNT] = "rowcount",
  [GTK_ACCESSIBLE_RELATION_ROW_INDEX] = "rowindex",
  [GTK_ACCESSIBLE_RELATION_ROW_INDEX_TEXT] = "rowindextext",
  [GTK_ACCESSIBLE_RELATION_ROW_SPAN] = "rowspan",
  [GTK_ACCESSIBLE_RELATION_SET_SIZE] = "setsize",
};

/*< private >
 * gtk_accessible_relation_set_print:
 * @self: a #GtkAccessibleRelationSet
 * @only_set: %TRUE if only the set relations should be printed
 * @buffer: a #GString
 *
 * Prints the contents of the #GtkAccessibleRelationSet into @buffer.
 */
void
gtk_accessible_relation_set_print (GtkAccessibleRelationSet *self,
                                   gboolean               only_set,
                                   GString               *buffer)
{
  if (only_set && _gtk_bitmask_is_empty (self->relation_set))
    {
      g_string_append (buffer, "{}");
      return;
    }

  g_string_append (buffer, "{\n");

  for (int i = 0; i < G_N_ELEMENTS (relation_names); i++)
    {
      if (only_set && !_gtk_bitmask_get (self->relation_set, i))
        continue;

      g_string_append (buffer, "    ");
      g_string_append (buffer, relation_names[i]);
      g_string_append (buffer, ": ");

      gtk_accessible_value_print (self->relation_values[i], buffer);

      g_string_append (buffer, ",\n");
    }

  g_string_append (buffer, "}");
}

/*< private >
 * gtk_accessible_relation_set_to_string:
 * @self: a #GtkAccessibleRelationSet
 *
 * Prints the contents of a #GtkAccessibleRelationSet into a string.
 *
 * Returns: (transfer full): a newly allocated string with the contents
 *   of the #GtkAccessibleRelationSet
 */
char *
gtk_accessible_relation_set_to_string (GtkAccessibleRelationSet *self)
{
  GString *buf = g_string_new (NULL);

  gtk_accessible_relation_set_print (self, TRUE, buf);

  return g_string_free (buf, FALSE);
}
