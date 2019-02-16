/* GTK+ - accessibility implementations
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#define GDK_COMPILATION
#include "gdk/gdkeventsprivate.h"

#include <glib/gi18n-lib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gtkpango.h"
#include "gtkentryaccessible.h"
#include "gtktextaccessible.h"
#include "gtkentryprivate.h"
#include "gtkcomboboxaccessible.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"

#define GTK_TYPE_ENTRY_ICON_ACCESSIBLE      (gtk_entry_icon_accessible_get_type ())
#define GTK_ENTRY_ICON_ACCESSIBLE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ENTRY_ICON_ACCESSIBLE, GtkEntryIconAccessible))
#define GTK_IS_ENTRY_ICON_ACCESSIBLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ENTRY_ICON_ACCESSIBLE))

struct _GtkEntryAccessiblePrivate
{
  AtkObject *icons[2];
  AtkObject *text;
};

typedef struct _GtkEntryIconAccessible GtkEntryIconAccessible;
typedef struct _GtkEntryIconAccessibleClass GtkEntryIconAccessibleClass;

struct _GtkEntryIconAccessible
{
  AtkObject parent;

  GtkEntryAccessible *entry;
  GtkEntryIconPosition pos;
};

struct _GtkEntryIconAccessibleClass
{
  AtkObjectClass parent_class;
};

static void icon_atk_action_interface_init (AtkActionIface *iface);
static void icon_atk_component_interface_init (AtkComponentIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkEntryIconAccessible, gtk_entry_icon_accessible, ATK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, icon_atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, icon_atk_component_interface_init))

static void
gtk_entry_icon_accessible_remove_entry (gpointer data, GObject *obj)
{
  GtkEntryIconAccessible *icon = data;

  if (icon->entry)
    {
      icon->entry = NULL;
      g_object_notify (G_OBJECT (icon), "accessible-parent");
      atk_object_notify_state_change (ATK_OBJECT (icon), ATK_STATE_DEFUNCT, TRUE);
    }
}

static AtkObject *
gtk_entry_icon_accessible_new (GtkEntryAccessible *entry,
                               GtkEntryIconPosition pos)
{
  GtkEntryIconAccessible *icon;
  AtkObject *accessible;

  icon = g_object_new (gtk_entry_icon_accessible_get_type (), NULL);
  icon->entry = entry;
  g_object_weak_ref (G_OBJECT (entry),
                     gtk_entry_icon_accessible_remove_entry,
                     icon);
  icon->pos = pos;

  accessible = ATK_OBJECT (icon);
  atk_object_initialize (accessible, NULL);
  return accessible;
}

static void
gtk_entry_icon_accessible_init (GtkEntryIconAccessible *icon)
{
}

static void
gtk_entry_icon_accessible_initialize (AtkObject *obj,
                                      gpointer   data)
{
  GtkEntryIconAccessible *icon = GTK_ENTRY_ICON_ACCESSIBLE (obj);
  GtkWidget *widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (icon->entry));
  GtkEntry *gtk_entry = GTK_ENTRY (widget);
  const gchar *name;
  gchar *text;

  ATK_OBJECT_CLASS (gtk_entry_icon_accessible_parent_class)->initialize (obj, data);
  atk_object_set_role (obj, ATK_ROLE_ICON);

  name = gtk_entry_get_icon_name (gtk_entry, icon->pos);
  if (name)
    atk_object_set_name (obj, name);

  text = gtk_entry_get_icon_tooltip_text (gtk_entry, icon->pos);
  if (text)
    {
      atk_object_set_description (obj, text);
      g_free (text);
    }

  atk_object_set_parent (obj, ATK_OBJECT (icon->entry));
}

static AtkObject *
gtk_entry_icon_accessible_get_parent (AtkObject *accessible)
{
  GtkEntryIconAccessible *icon = GTK_ENTRY_ICON_ACCESSIBLE (accessible);

  return ATK_OBJECT (icon->entry);
}

static AtkStateSet *
gtk_entry_icon_accessible_ref_state_set (AtkObject *accessible)
{
  GtkEntryIconAccessible *icon = GTK_ENTRY_ICON_ACCESSIBLE (accessible);
  AtkStateSet *set = atk_state_set_new ();
  AtkStateSet *entry_set;
  GtkWidget *widget;
  GtkEntry *gtk_entry;

  if (!icon->entry)
    {
      atk_state_set_add_state (set, ATK_STATE_DEFUNCT);
      return set;
    }

  entry_set = atk_object_ref_state_set (ATK_OBJECT (icon->entry));
  if (!entry_set || atk_state_set_contains_state (entry_set, ATK_STATE_DEFUNCT))
    {
      atk_state_set_add_state (set, ATK_STATE_DEFUNCT);
    g_clear_object (&entry_set);
      return set;
    }

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (icon->entry));
  gtk_entry = GTK_ENTRY (widget);

  if (atk_state_set_contains_state (entry_set, ATK_STATE_ENABLED))
    atk_state_set_add_state (set, ATK_STATE_ENABLED);
  if (atk_state_set_contains_state (entry_set, ATK_STATE_SENSITIVE))
    atk_state_set_add_state (set, ATK_STATE_SENSITIVE);
  if (atk_state_set_contains_state (entry_set, ATK_STATE_SHOWING))
    atk_state_set_add_state (set, ATK_STATE_SHOWING);
  if (atk_state_set_contains_state (entry_set, ATK_STATE_VISIBLE))
    atk_state_set_add_state (set, ATK_STATE_VISIBLE);

  if (!gtk_entry_get_icon_sensitive (gtk_entry, icon->pos))
      atk_state_set_remove_state (set, ATK_STATE_SENSITIVE);
  if (!gtk_entry_get_icon_activatable (gtk_entry, icon->pos))
      atk_state_set_remove_state (set, ATK_STATE_ENABLED);

  g_object_unref (entry_set);
  return set;
}

static void
gtk_entry_icon_accessible_invalidate (GtkEntryIconAccessible *icon)
{
  if (!icon->entry)
    return;
  g_object_weak_unref (G_OBJECT (icon->entry),
                       gtk_entry_icon_accessible_remove_entry,
                       icon);
  gtk_entry_icon_accessible_remove_entry (icon, NULL);
}

static void
gtk_entry_icon_accessible_finalize (GObject *object)
{
  GtkEntryIconAccessible *icon = GTK_ENTRY_ICON_ACCESSIBLE (object);

  gtk_entry_icon_accessible_invalidate (icon);

  G_OBJECT_CLASS (gtk_entry_icon_accessible_parent_class)->finalize (object);
}

static void
gtk_entry_icon_accessible_class_init (GtkEntryIconAccessibleClass *klass)
{
  AtkObjectClass  *atk_class = ATK_OBJECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  atk_class->initialize = gtk_entry_icon_accessible_initialize;
  atk_class->get_parent = gtk_entry_icon_accessible_get_parent;
  atk_class->ref_state_set = gtk_entry_icon_accessible_ref_state_set;

  gobject_class->finalize = gtk_entry_icon_accessible_finalize;
}

static gboolean
gtk_entry_icon_accessible_do_action (AtkAction *action,
                                     gint       i)
{
  GtkEntryIconAccessible *icon = (GtkEntryIconAccessible *)action;
  GtkWidget *widget;
  GtkEntry *gtk_entry;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (icon->entry));
  if (widget == NULL)
    return FALSE;

  if (i != 0)
    return FALSE;

  if (!gtk_widget_is_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  gtk_entry = GTK_ENTRY (widget);

  if (!gtk_entry_get_icon_sensitive (gtk_entry, icon->pos) ||
      !gtk_entry_get_icon_activatable (gtk_entry, icon->pos))
    return FALSE;

  g_signal_emit_by_name (widget, "icon-press", 0, icon->pos);
  return TRUE;
}

static gint
gtk_entry_icon_accessible_get_n_actions (AtkAction *action)
{
  GtkEntryIconAccessible *icon = GTK_ENTRY_ICON_ACCESSIBLE (action);
  GtkWidget *widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (icon->entry));
  GtkEntry *gtk_entry = GTK_ENTRY (widget);

  return (gtk_entry_get_icon_activatable (gtk_entry, icon->pos) ? 1 : 0);
}

static const gchar *
gtk_entry_icon_accessible_get_name (AtkAction *action,
                                    gint       i)
{
  GtkEntryIconAccessible *icon = GTK_ENTRY_ICON_ACCESSIBLE (action);
  GtkWidget *widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (icon->entry));
  GtkEntry *gtk_entry = GTK_ENTRY (widget);

  if (i != 0)
    return NULL;
  if (!gtk_entry_get_icon_activatable (gtk_entry, icon->pos))
    return NULL;

  return "activate";
}

static void
icon_atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_entry_icon_accessible_do_action;
  iface->get_n_actions = gtk_entry_icon_accessible_get_n_actions;
  iface->get_name = gtk_entry_icon_accessible_get_name;
}

static void
gtk_entry_icon_accessible_get_extents (AtkComponent   *component,
                                       gint           *x,
                                       gint           *y,
                                       gint           *width,
                                       gint           *height,
                                       AtkCoordType    coord_type)
{
  GtkEntryIconAccessible *icon = GTK_ENTRY_ICON_ACCESSIBLE (component);
  GdkRectangle icon_area;
  GtkEntry *gtk_entry;
  GtkWidget *widget;

  *x = G_MININT;
  atk_component_get_extents (ATK_COMPONENT (icon->entry), x, y, width, height,
                             coord_type);
  if (*x == G_MININT)
    return;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (icon->entry));
  gtk_entry = GTK_ENTRY (widget);
  gtk_entry_get_icon_area (gtk_entry, icon->pos, &icon_area);
  *width = icon_area.width;
  *height = icon_area.height;
  *x += icon_area.x;
  *y += icon_area.y;
}

static void
gtk_entry_icon_accessible_get_position (AtkComponent   *component,
                                        gint           *x,
                                        gint           *y,
                                        AtkCoordType    coord_type)
{
  GtkEntryIconAccessible *icon = GTK_ENTRY_ICON_ACCESSIBLE (component);
  GdkRectangle icon_area;
  GtkEntry *gtk_entry;
  GtkWidget *widget;

  *x = G_MININT;
  atk_component_get_extents (ATK_COMPONENT (icon->entry), x, y, NULL, NULL,
                             coord_type);
  if (*x == G_MININT)
    return;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (icon->entry));
  gtk_entry = GTK_ENTRY (widget);
  gtk_entry_get_icon_area (gtk_entry, icon->pos, &icon_area);
  *x += icon_area.x;
  *y += icon_area.y;
}

static void
gtk_entry_icon_accessible_get_size (AtkComponent *component,
                                gint         *width,
                                gint         *height)
{
  GtkEntryIconAccessible *icon = GTK_ENTRY_ICON_ACCESSIBLE (component);
  GdkRectangle icon_area;
  GtkEntry *gtk_entry;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (icon->entry));
  gtk_entry = GTK_ENTRY (widget);
  gtk_entry_get_icon_area (gtk_entry, icon->pos, &icon_area);
  *width = icon_area.width;
  *height = icon_area.height;
}

static void
icon_atk_component_interface_init (AtkComponentIface *iface)
{
  iface->get_extents = gtk_entry_icon_accessible_get_extents;
  iface->get_size = gtk_entry_icon_accessible_get_size;
  iface->get_position = gtk_entry_icon_accessible_get_position;
}

/* Callbacks */

G_DEFINE_TYPE_WITH_CODE (GtkEntryAccessible, gtk_entry_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkEntryAccessible))


static AtkStateSet *
gtk_entry_accessible_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return NULL;

  state_set = ATK_OBJECT_CLASS (gtk_entry_accessible_parent_class)->ref_state_set (accessible);

  return state_set;
}

static AtkAttributeSet *
gtk_entry_accessible_get_attributes (AtkObject *accessible)
{
  GtkWidget *widget;
  AtkAttributeSet *attributes;

  attributes = ATK_OBJECT_CLASS (gtk_entry_accessible_parent_class)->get_attributes (accessible);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return attributes;

  return attributes;
}

static void
gtk_entry_accessible_initialize (AtkObject *obj,
                                 gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_entry_accessible_parent_class)->initialize (obj, data);
}

static void
gtk_entry_accessible_notify_gtk (GObject    *obj,
                                 GParamSpec *pspec)
{
  GtkWidget *widget;
  AtkObject* atk_obj;
  GtkEntry* gtk_entry;
  GtkEntryAccessible* entry;
  GtkEntryAccessiblePrivate *priv;

  widget = GTK_WIDGET (obj);
  atk_obj = gtk_widget_get_accessible (widget);
  gtk_entry = GTK_ENTRY (widget);
  entry = GTK_ENTRY_ACCESSIBLE (atk_obj);
  priv = entry->priv;

  if (g_strcmp0 (pspec->name, "primary-icon-storage-type") == 0)
    {
      if (gtk_entry_get_icon_storage_type (gtk_entry, GTK_ENTRY_ICON_PRIMARY) != GTK_IMAGE_EMPTY && !priv->icons[GTK_ENTRY_ICON_PRIMARY])
        {
          priv->icons[GTK_ENTRY_ICON_PRIMARY] = gtk_entry_icon_accessible_new (entry, GTK_ENTRY_ICON_PRIMARY);
          g_signal_emit_by_name (entry, "children-changed::add", 0,
                                 priv->icons[GTK_ENTRY_ICON_PRIMARY], NULL);
        }
      else if (gtk_entry_get_icon_storage_type (gtk_entry, GTK_ENTRY_ICON_PRIMARY) == GTK_IMAGE_EMPTY && priv->icons[GTK_ENTRY_ICON_PRIMARY])
        {
          gtk_entry_icon_accessible_invalidate (GTK_ENTRY_ICON_ACCESSIBLE (priv->icons[GTK_ENTRY_ICON_PRIMARY]));
          g_signal_emit_by_name (entry, "children-changed::remove", 0,
                                 priv->icons[GTK_ENTRY_ICON_PRIMARY], NULL);
          g_clear_object (&priv->icons[GTK_ENTRY_ICON_PRIMARY]);
        }
    }
  else if (g_strcmp0 (pspec->name, "secondary-icon-storage-type") == 0)
    {
      gint index = (priv->icons[GTK_ENTRY_ICON_PRIMARY] ? 1 : 0);
      if (gtk_entry_get_icon_storage_type (gtk_entry, GTK_ENTRY_ICON_SECONDARY) != GTK_IMAGE_EMPTY && !priv->icons[GTK_ENTRY_ICON_SECONDARY])
        {
          priv->icons[GTK_ENTRY_ICON_SECONDARY] = gtk_entry_icon_accessible_new (entry, GTK_ENTRY_ICON_SECONDARY);
          g_signal_emit_by_name (entry, "children-changed::add", index,
                                 priv->icons[GTK_ENTRY_ICON_SECONDARY], NULL);
        }
      else if (gtk_entry_get_icon_storage_type (gtk_entry, GTK_ENTRY_ICON_SECONDARY) == GTK_IMAGE_EMPTY && priv->icons[GTK_ENTRY_ICON_SECONDARY])
        {
          gtk_entry_icon_accessible_invalidate (GTK_ENTRY_ICON_ACCESSIBLE (priv->icons[GTK_ENTRY_ICON_SECONDARY]));
          g_signal_emit_by_name (entry, "children-changed::remove", index,
                                 priv->icons[GTK_ENTRY_ICON_SECONDARY], NULL);
          g_clear_object (&priv->icons[GTK_ENTRY_ICON_SECONDARY]);
        }
    }
  else if (g_strcmp0 (pspec->name, "primary-icon-name") == 0)
    {
      if (priv->icons[GTK_ENTRY_ICON_PRIMARY])
        {
          const gchar *name;
          name = gtk_entry_get_icon_name (gtk_entry,
                                          GTK_ENTRY_ICON_PRIMARY);
          if (name)
            atk_object_set_name (priv->icons[GTK_ENTRY_ICON_PRIMARY], name);
        }
    }
  else if (g_strcmp0 (pspec->name, "secondary-icon-name") == 0)
    {
      if (priv->icons[GTK_ENTRY_ICON_SECONDARY])
        {
          const gchar *name;
          name = gtk_entry_get_icon_name (gtk_entry,
                                          GTK_ENTRY_ICON_SECONDARY);
          if (name)
            atk_object_set_name (priv->icons[GTK_ENTRY_ICON_SECONDARY], name);
        }
    }
  else if (g_strcmp0 (pspec->name, "primary-icon-tooltip-text") == 0)
    {
      if (priv->icons[GTK_ENTRY_ICON_PRIMARY])
        {
          gchar *text;
          text = gtk_entry_get_icon_tooltip_text (gtk_entry,
                                                    GTK_ENTRY_ICON_PRIMARY);
          if (text)
            {
              atk_object_set_description (priv->icons[GTK_ENTRY_ICON_PRIMARY],
                                      text);
              g_free (text);
            }
          else
            {
              atk_object_set_description (priv->icons[GTK_ENTRY_ICON_PRIMARY],
                                      "");
            }
        }
    }
  else if (g_strcmp0 (pspec->name, "secondary-icon-tooltip-text") == 0)
    {
      if (priv->icons[GTK_ENTRY_ICON_SECONDARY])
        {
          gchar *text;
          text = gtk_entry_get_icon_tooltip_text (gtk_entry,
                                                    GTK_ENTRY_ICON_SECONDARY);
          if (text)
            {
              atk_object_set_description (priv->icons[GTK_ENTRY_ICON_SECONDARY],
                                      text);
              g_free (text);
            }
          else
            {
              atk_object_set_description (priv->icons[GTK_ENTRY_ICON_SECONDARY],
                                      "");
            }
        }
    }
  else if (g_strcmp0 (pspec->name, "primary-icon-activatable") == 0)
    {
      if (priv->icons[GTK_ENTRY_ICON_PRIMARY])
        {
          gboolean on = gtk_entry_get_icon_activatable (gtk_entry, GTK_ENTRY_ICON_PRIMARY);
          atk_object_notify_state_change (priv->icons[GTK_ENTRY_ICON_PRIMARY],
                                          ATK_STATE_ENABLED, on);
        }
    }
  else if (g_strcmp0 (pspec->name, "secondary-icon-activatable") == 0)
    {
      if (priv->icons[GTK_ENTRY_ICON_SECONDARY])
        {
          gboolean on = gtk_entry_get_icon_activatable (gtk_entry, GTK_ENTRY_ICON_SECONDARY);
          atk_object_notify_state_change (priv->icons[GTK_ENTRY_ICON_SECONDARY],
                                          ATK_STATE_ENABLED, on);
        }
    }
  else if (g_strcmp0 (pspec->name, "primary-icon-sensitive") == 0)
    {
      if (priv->icons[GTK_ENTRY_ICON_PRIMARY])
        {
          gboolean on = gtk_entry_get_icon_sensitive (gtk_entry, GTK_ENTRY_ICON_PRIMARY);
          atk_object_notify_state_change (priv->icons[GTK_ENTRY_ICON_PRIMARY],
                                          ATK_STATE_SENSITIVE, on);
        }
    }
  else if (g_strcmp0 (pspec->name, "secondary-icon-sensitive") == 0)
    {
      if (priv->icons[GTK_ENTRY_ICON_SECONDARY])
        {
          gboolean on = gtk_entry_get_icon_sensitive (gtk_entry, GTK_ENTRY_ICON_SECONDARY);
          atk_object_notify_state_change (priv->icons[GTK_ENTRY_ICON_SECONDARY],
                                          ATK_STATE_SENSITIVE, on);
        }
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (gtk_entry_accessible_parent_class)->notify_gtk (obj, pspec);
}

static gint
gtk_entry_accessible_get_index_in_parent (AtkObject *accessible)
{
  /*
   * If the parent widget is a combo box then the index is 1
   * otherwise do the normal thing.
   */
  if (accessible->accessible_parent)
    if (GTK_IS_COMBO_BOX_ACCESSIBLE (accessible->accessible_parent))
      return 1;

  return ATK_OBJECT_CLASS (gtk_entry_accessible_parent_class)->get_index_in_parent (accessible);
}

static gint
gtk_entry_accessible_get_n_children (AtkObject* obj)
{
  GtkWidget *widget;
  GtkEntry *entry;
  gint count = 1;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return 0;

  entry = GTK_ENTRY (widget);

  if (gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_PRIMARY) != GTK_IMAGE_EMPTY)
    count++;
  if (gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_SECONDARY) != GTK_IMAGE_EMPTY)
    count++;
  return count;
}

static AtkObject *
gtk_entry_accessible_ref_child (AtkObject *obj,
                                gint i)
{
  GtkEntryAccessible *accessible = GTK_ENTRY_ACCESSIBLE (obj);
  GtkEntryAccessiblePrivate *priv = accessible->priv;
  GtkWidget *widget;
  GtkEntry *entry;
  int pos;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  entry = GTK_ENTRY (widget);

  switch (i)
    {
    case 0:
      if (gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_PRIMARY) != GTK_IMAGE_EMPTY)
        pos = GTK_ENTRY_ICON_PRIMARY;
      else
        pos = -1;
      break;

    case 1:
      if (gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_PRIMARY) != GTK_IMAGE_EMPTY)
        pos = -1;
      else if (gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_SECONDARY) == GTK_IMAGE_EMPTY)
        pos = GTK_ENTRY_ICON_SECONDARY;
      else
        return NULL;
      break;

    case 2:
      if (gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_PRIMARY) == GTK_IMAGE_EMPTY)
        return NULL;
      else if (gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_SECONDARY) != GTK_IMAGE_EMPTY)
        pos = GTK_ENTRY_ICON_SECONDARY;
      else
        return NULL;
      break;

    default:
      return NULL;
    }

  if (pos == -1)
    {
      if (!priv->text)
        priv->text = g_object_new (GTK_TYPE_TEXT_ACCESSIBLE,
                                   "widget", gtk_entry_get_text_widget (entry),
                                   NULL);
      return g_object_ref (priv->text);
    }
  else
    {
      if (!priv->icons[pos])
        priv->icons[pos] = gtk_entry_icon_accessible_new (accessible, pos);
      return g_object_ref (priv->icons[pos]);
    }
}

static void
gtk_entry_accessible_finalize (GObject *object)
{
  GtkEntryAccessible *entry = GTK_ENTRY_ACCESSIBLE (object);
  GtkEntryAccessiblePrivate *priv = entry->priv;

  g_clear_object (&priv->icons[GTK_ENTRY_ICON_PRIMARY]);
  g_clear_object (&priv->icons[GTK_ENTRY_ICON_SECONDARY]);
  g_clear_object (&priv->text);

  G_OBJECT_CLASS (gtk_entry_accessible_parent_class)->finalize (object);
}

static void
gtk_entry_accessible_class_init (GtkEntryAccessibleClass *klass)
{
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);
  GtkWidgetAccessibleClass *widget_class = (GtkWidgetAccessibleClass*)klass;
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  class->ref_state_set = gtk_entry_accessible_ref_state_set;
  class->get_index_in_parent = gtk_entry_accessible_get_index_in_parent;
  class->initialize = gtk_entry_accessible_initialize;
  class->get_attributes = gtk_entry_accessible_get_attributes;
  class->get_n_children = gtk_entry_accessible_get_n_children;
  class->ref_child = gtk_entry_accessible_ref_child;

  widget_class->notify_gtk = gtk_entry_accessible_notify_gtk;

  gobject_class->finalize = gtk_entry_accessible_finalize;
}

static void
gtk_entry_accessible_init (GtkEntryAccessible *entry)
{
  entry->priv = gtk_entry_accessible_get_instance_private (entry);
}
