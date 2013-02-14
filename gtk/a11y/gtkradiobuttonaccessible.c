/* GTK+ - accessibility implementations
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtkradiobuttonaccessible.h"

struct _GtkRadioButtonAccessiblePrivate
{
  GSList *old_group;
};


G_DEFINE_TYPE (GtkRadioButtonAccessible, gtk_radio_button_accessible, GTK_TYPE_TOGGLE_BUTTON_ACCESSIBLE)

static void
gtk_radio_button_accessible_initialize (AtkObject *accessible,
                                        gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_radio_button_accessible_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_RADIO_BUTTON;
}

static AtkRelationSet *
gtk_radio_button_accessible_ref_relation_set (AtkObject *obj)
{
  GtkWidget *widget;
  AtkRelationSet *relation_set;
  GSList *list;
  GtkRadioButtonAccessible *radio_button;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  radio_button = GTK_RADIO_BUTTON_ACCESSIBLE (obj);

  relation_set = ATK_OBJECT_CLASS (gtk_radio_button_accessible_parent_class)->ref_relation_set (obj);

  /* If the radio button'group has changed remove the relation */
  list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

  if (radio_button->priv->old_group != list)
    {
      AtkRelation *relation;

      relation = atk_relation_set_get_relation_by_type (relation_set, ATK_RELATION_MEMBER_OF);
      atk_relation_set_remove (relation_set, relation);
    }

  if (!atk_relation_set_contains (relation_set, ATK_RELATION_MEMBER_OF))
  {
    /*
     * Get the members of the button group
     */
    radio_button->priv->old_group = list;
    if (list)
      {
        AtkObject **accessible_array;
        guint list_length;
        AtkRelation* relation;
        gint i = 0;

        list_length = g_slist_length (list);
        accessible_array = g_new (AtkObject *, list_length);
        while (list != NULL)
          {
            GtkWidget* list_item = list->data;

            accessible_array[i++] = gtk_widget_get_accessible (list_item);

            list = list->next;
          }
        relation = atk_relation_new (accessible_array, list_length,
                                     ATK_RELATION_MEMBER_OF);
        g_free (accessible_array);

        atk_relation_set_add (relation_set, relation);
        /*
         * Unref the relation so that it is not leaked.
         */
        g_object_unref (relation);
      }
    }

  return relation_set;
}

static void
gtk_radio_button_accessible_class_init (GtkRadioButtonAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gtk_radio_button_accessible_initialize;
  class->ref_relation_set = gtk_radio_button_accessible_ref_relation_set;

  g_type_class_add_private (klass, sizeof (GtkRadioButtonAccessiblePrivate));
}

static void
gtk_radio_button_accessible_init (GtkRadioButtonAccessible *radio_button)
{
  radio_button->priv = G_TYPE_INSTANCE_GET_PRIVATE (radio_button,
                                                    GTK_TYPE_RADIO_BUTTON_ACCESSIBLE,
                                                    GtkRadioButtonAccessiblePrivate);
}
