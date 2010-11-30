/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include "gtkradiogroupprivate.h"
#include "gtkradioaction.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"

/**
 * SECTION:gtkradiogroup
 * @Short_description: A group of object having radiobutton like behaviour
 * @Title: GtkRadioGroup
 * @See_also: #GtkRadioButton, #GtkRadioAction, #GtkRadioToolButton, #GtkRadioMenuItem
 *
 * #GtkRadioButton, and several other similar widgets are grouped together in logical
 * groups (unrelated to how they are laid out in the user interface) such that only
 * one item in the group can be active at any one time. This grouping is represented
 * by the #GtkRadioGroup class.
 *
 * #GtkRadioGroup has operations that let you list the items and get the active item.
 * There are also signals that signal when the group content changes and when the
 * active item changes.
 *
 * Since: 3.0
 */

struct _GtkRadioGroupPrivate
{
  GSList *items;
  GObject *active;
};

enum {
  PROP_0,
  PROP_ACTIVE_ITEM,
  PROP_ACTIVE_VALUE
};


static void gtk_radio_group_set_property (GObject      *object,
					  guint         prop_id,
					  const GValue *value,
					  GParamSpec   *pspec);
static void gtk_radio_group_get_property (GObject      *object,
					  guint         prop_id,
					  GValue       *value,
					  GParamSpec   *pspec);

G_DEFINE_TYPE (GtkRadioGroup, gtk_radio_group, G_TYPE_INITIALLY_UNOWNED)

enum {
  ACTIVE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
gtk_radio_group_class_init (GtkRadioGroupClass *class)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_radio_group_set_property;
  gobject_class->get_property = gtk_radio_group_get_property;

  /**
   * GtkRadioGroup:active-item:
   *
   * Gets the active item in a radio group.
   */
  g_object_class_install_property (gobject_class,
				   PROP_ACTIVE_ITEM,
				   g_param_spec_object ("active-item",
							P_("Active item"),
							P_("The active item in the radio group."),
							G_TYPE_OBJECT,
							GTK_PARAM_READABLE));
  class->active_changed = NULL;

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE_VALUE,
                                   g_param_spec_string ("active-value",
                                                        P_("Active value"),
                                                        P_("The value of the active item in the radio group"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkRadioGroup::active-changed:
   * @radio_group: the radio group on which the signal is emitted
   * @active: the new active item
   *
   * The ::active-changed signal gets emitted when there are changes
   * in what item is the active one.
   */
  signals[ACTIVE_CHANGED] = g_signal_new (I_("active-changed"),
					  G_OBJECT_CLASS_TYPE (gobject_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (GtkRadioGroupClass, active_changed),
					  NULL, NULL,
					  _gtk_marshal_VOID__OBJECT,
					  G_TYPE_NONE, 1, G_TYPE_OBJECT);

  g_type_class_add_private (class, sizeof (GtkRadioGroupPrivate));
}

static void
gtk_radio_group_init (GtkRadioGroup *radio_group)
{
  GtkRadioGroupPrivate *priv;

  radio_group->priv = G_TYPE_INSTANCE_GET_PRIVATE (radio_group,
						   GTK_TYPE_RADIO_GROUP,
						   GtkRadioGroupPrivate);
  priv = radio_group->priv;

  priv->items = NULL;
  priv->active = NULL;
}

static void
gtk_radio_group_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  GtkRadioGroup *radio_group;

  radio_group = GTK_RADIO_GROUP (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_VALUE:
      gtk_radio_group_set_active_value (radio_group, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_radio_group_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  GtkRadioGroup *radio_group;

  radio_group = GTK_RADIO_GROUP (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_ITEM:
      g_value_set_object (value, radio_group->priv->active);
      break;
    case PROP_ACTIVE_VALUE:
      g_value_set_string (value, gtk_radio_group_get_active_value (radio_group));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_radio_group_new:
 *
 * Creates a new empty #GtkRadioGroup.
 *
 * Note that #GtkRadioGroup inherits from #GInitiallyUnowned, so
 * the newly constructed group is floating. Typically adding an
 * item to the group will sink it as the item will own the group, but
 * if you need to keep around the group without adding anything to
 * it you must call g_object_ref_sink() to get an owning reference
 * to it.
 *
 * Returns: a new radio group
 */
GtkRadioGroup*
gtk_radio_group_new (void)
{
  GtkRadioGroup *radio_group;

  radio_group = g_object_new (GTK_TYPE_RADIO_GROUP, NULL);

  return radio_group;
}

/**
 * gtk_radio_group_get_items:
 * @radio_group: a #GtkRadioGroup.
 *
 * Returns a list with the items in the #GtkRadioGroup.
 *
 * Return value: (transfer container) (element-type GObject): a
 * newly-allocated #GSList containing all the items in the group.
 * It should be freed by g_slist_free().
 */
GSList*
gtk_radio_group_get_items (GtkRadioGroup *radio_group)
{
  g_return_val_if_fail (GTK_IS_RADIO_GROUP (radio_group), NULL);

  return g_slist_copy (radio_group->priv->items);
}

/**
 * gtk_radio_group_get_items:
 * @radio_group: a #GtkRadioGroup.
 *
 * Retrieves the currently active item in the group.
 *
 * Return value: (transfer none): pointer to a #GObject that
 *    is the active item in the group, or %NULL if the group
 *    is empty.
 */
GObject *
gtk_radio_group_get_active_item (GtkRadioGroup *radio_group)
{
  g_return_val_if_fail (GTK_IS_RADIO_GROUP (radio_group), NULL);

  return radio_group->priv->active;
}

gboolean
_gtk_radio_group_is_empty (GtkRadioGroup *radio_group)
{
  return radio_group->priv->items == NULL;
}

GObject *
_gtk_radio_group_get_singleton (GtkRadioGroup *radio_group)
{
  GtkRadioGroupPrivate *priv;

  priv = radio_group->priv;
  if (priv->items != NULL && priv->items->next == NULL)
    return g_object_ref (priv->items->data);
  return NULL;
}

/* Caller must handle signal emission if the active item
   changes */
void
_gtk_radio_group_add_item (GtkRadioGroup *radio_group,
			   GObject       *item)
{
  GtkRadioGroupPrivate *priv;

  g_return_if_fail (GTK_IS_RADIO_GROUP (radio_group));
  g_return_if_fail (!g_slist_find (radio_group->priv->items, item));

  priv = radio_group->priv;
  priv->items = g_slist_prepend (priv->items, item);
  if (priv->active == NULL)
    priv->active = item;
}

/* Caller must handle signal emission if the active item
   changes */
void
_gtk_radio_group_remove_item (GtkRadioGroup *radio_group,
			      GObject       *item)
{
  GtkRadioGroupPrivate *priv;

  g_return_if_fail (GTK_IS_RADIO_GROUP (radio_group));
  g_return_if_fail (g_slist_find (radio_group->priv->items, item));

  priv = radio_group->priv;
  priv->items = g_slist_remove (priv->items, item);

  if (priv->active == item)
    {
      if (priv->items)
	priv->active = priv->items->data;
      else
	priv->active = NULL;
    }
}

void
_gtk_radio_group_set_active_item (GtkRadioGroup *radio_group,
				  GObject       *item)
{
  radio_group->priv->active = item;
}

void
_gtk_radio_group_emit_active_changed (GtkRadioGroup *radio_group)
{
  g_object_notify (G_OBJECT (radio_group), "active-item");
  g_object_notify (G_OBJECT (radio_group), "active-value");
  g_signal_emit (radio_group, signals[ACTIVE_CHANGED], 0, radio_group->priv->active);
}

void
gtk_radio_group_set_active_value (GtkRadioGroup *radio_group,
                                  const gchar   *value)
{
  GSList *l;

  for (l = radio_group->priv->items; l; l = l->next)
    {
      GObject *item = l->data;

      if (GTK_IS_RADIO_ACTION (item))
        {
          const gchar *s;

          s = gtk_radio_action_get_string_value (GTK_RADIO_ACTION (item));

          if (strcmp (value, s) == 0)
            {
              _gtk_radio_group_set_active_item (radio_group, item);
              _gtk_radio_group_emit_active_changed (radio_group);

              break;
            }
        }
    }
}

const gchar *
gtk_radio_group_get_active_value (GtkRadioGroup *radio_group)
{
  if (GTK_IS_RADIO_ACTION (radio_group->priv->active))
    return gtk_radio_action_get_string_value (GTK_RADIO_ACTION (radio_group->priv->active));
  else
    return NULL;
}
