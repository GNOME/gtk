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

#include <glib/gi18n-lib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gtkpango.h"
#include "gtkentryaccessible.h"
#include "gtkentryprivate.h"
#include "gtkcomboboxaccessible.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"

#define GTK_TYPE_ENTRY_ICON_ACCESSIBLE      (gtk_entry_icon_accessible_get_type ())
#define GTK_ENTRY_ICON_ACCESSIBLE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ENTRY_ICON_ACCESSIBLE, GtkEntryIconAccessible))
#define GTK_IS_ENTRY_ICON_ACCESSIBLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ENTRY_ICON_ACCESSIBLE))

struct _GtkEntryAccessiblePrivate
{
  gint cursor_position;
  gint selection_bound;
  AtkObject *icons[2];
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

static void atk_action_interface_init (AtkActionIface *iface);

static void icon_atk_action_interface_init (AtkActionIface *iface);
static void icon_atk_component_interface_init (AtkComponentIface *iface);

GType gtk_entry_icon_accessible_get_type (void);

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
  GdkEvent event;
  GdkRectangle icon_area;

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

  gtk_entry_get_icon_area (gtk_entry, icon->pos, &icon_area);
  memset (&event, 0, sizeof (event));
  event.button.type = GDK_BUTTON_PRESS;
  event.button.window = gtk_widget_get_window (widget);
  event.button.button = 1;
  event.button.send_event = TRUE;
  event.button.time = GDK_CURRENT_TIME;
  event.button.x = icon_area.x;
  event.button.y = icon_area.y;

  g_signal_emit_by_name (widget, "icon-press", 0, icon->pos, &event);
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

static void     insert_text_cb             (GtkEditable        *editable,
                                            gchar              *new_text,
                                            gint                new_text_length,
                                            gint               *position);
static void     delete_text_cb             (GtkEditable        *editable,
                                            gint                start,
                                            gint                end);

static gboolean check_for_selection_change (GtkEntryAccessible *entry,
                                            GtkEntry           *gtk_entry);


static void atk_editable_text_interface_init (AtkEditableTextIface *iface);
static void atk_text_interface_init          (AtkTextIface         *iface);
static void atk_action_interface_init        (AtkActionIface       *iface);


G_DEFINE_TYPE_WITH_CODE (GtkEntryAccessible, gtk_entry_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkEntryAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_EDITABLE_TEXT, atk_editable_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init))


static AtkStateSet *
gtk_entry_accessible_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  gboolean value;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return NULL;

  state_set = ATK_OBJECT_CLASS (gtk_entry_accessible_parent_class)->ref_state_set (accessible);

  g_object_get (G_OBJECT (widget), "editable", &value, NULL);
  if (value)
    atk_state_set_add_state (state_set, ATK_STATE_EDITABLE);
  atk_state_set_add_state (state_set, ATK_STATE_SINGLE_LINE);

  return state_set;
}

static AtkAttributeSet *
gtk_entry_accessible_get_attributes (AtkObject *accessible)
{
  GtkWidget *widget;
  AtkAttributeSet *attributes;
  AtkAttribute *placeholder_text;
  const gchar *text;

  attributes = ATK_OBJECT_CLASS (gtk_entry_accessible_parent_class)->get_attributes (accessible);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return attributes;

  text = gtk_entry_get_placeholder_text (GTK_ENTRY (widget));
  if (text == NULL)
    return attributes;

  placeholder_text = g_malloc (sizeof (AtkAttribute));
  placeholder_text->name = g_strdup ("placeholder-text");
  placeholder_text->value = g_strdup (text);

  attributes = g_slist_append (attributes, placeholder_text);

  return attributes;
}

static void
gtk_entry_accessible_initialize (AtkObject *obj,
                                 gpointer   data)
{
  GtkEntry *entry;
  GtkEntryAccessible *gtk_entry_accessible;
  gint start_pos, end_pos;

  ATK_OBJECT_CLASS (gtk_entry_accessible_parent_class)->initialize (obj, data);

  gtk_entry_accessible = GTK_ENTRY_ACCESSIBLE (obj);

  entry = GTK_ENTRY (data);
  gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start_pos, &end_pos);
  gtk_entry_accessible->priv->cursor_position = end_pos;
  gtk_entry_accessible->priv->selection_bound = start_pos;

  /* Set up signal callbacks */
  g_signal_connect_after (entry, "insert-text", G_CALLBACK (insert_text_cb), NULL);
  g_signal_connect (entry, "delete-text", G_CALLBACK (delete_text_cb), NULL);

  if (gtk_entry_get_visibility (entry))
    obj->role = ATK_ROLE_TEXT;
  else
    obj->role = ATK_ROLE_PASSWORD_TEXT;
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

  if (g_strcmp0 (pspec->name, "cursor-position") == 0)
    {
      if (check_for_selection_change (entry, gtk_entry))
        g_signal_emit_by_name (atk_obj, "text-selection-changed");
      /*
       * The entry cursor position has moved so generate the signal.
       */
      g_signal_emit_by_name (atk_obj, "text-caret-moved",
                             entry->priv->cursor_position);
    }
  else if (g_strcmp0 (pspec->name, "selection-bound") == 0)
    {
      if (check_for_selection_change (entry, gtk_entry))
        g_signal_emit_by_name (atk_obj, "text-selection-changed");
    }
  else if (g_strcmp0 (pspec->name, "editable") == 0)
    {
      gboolean value;

      g_object_get (obj, "editable", &value, NULL);
      atk_object_notify_state_change (atk_obj, ATK_STATE_EDITABLE, value);
    }
  else if (g_strcmp0 (pspec->name, "visibility") == 0)
    {
      gboolean visibility;
      AtkRole new_role;

      visibility = gtk_entry_get_visibility (gtk_entry);
      new_role = visibility ? ATK_ROLE_TEXT : ATK_ROLE_PASSWORD_TEXT;
      atk_object_set_role (atk_obj, new_role);
    }
  else if (g_strcmp0 (pspec->name, "primary-icon-storage-type") == 0)
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
  gint count = 0;

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
  GtkEntryIconPosition pos;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  entry = GTK_ENTRY (widget);

  switch (i)
    {
    case 0:
      if (gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_PRIMARY) != GTK_IMAGE_EMPTY)
        pos = GTK_ENTRY_ICON_PRIMARY;
      else if (gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_SECONDARY) != GTK_IMAGE_EMPTY)
        pos = GTK_ENTRY_ICON_SECONDARY;
      else
        return NULL;
      break;
    case 1:
      if (gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_PRIMARY) == GTK_IMAGE_EMPTY)
        return NULL;
      if (gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_SECONDARY) == GTK_IMAGE_EMPTY)
        return NULL;
      pos = GTK_ENTRY_ICON_SECONDARY;
      break;
    default:
      return NULL;
    }

  if (!priv->icons[pos])
    priv->icons[pos] = gtk_entry_icon_accessible_new (accessible, pos);
  return g_object_ref (priv->icons[pos]);
}

static void
gtk_entry_accessible_finalize (GObject *object)
{
  GtkEntryAccessible *entry = GTK_ENTRY_ACCESSIBLE (object);
  GtkEntryAccessiblePrivate *priv = entry->priv;

  g_clear_object (&priv->icons[GTK_ENTRY_ICON_PRIMARY]);
  g_clear_object (&priv->icons[GTK_ENTRY_ICON_SECONDARY]);

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
  entry->priv->cursor_position = 0;
  entry->priv->selection_bound = 0;
}

static gchar *
gtk_entry_accessible_get_text (AtkText *atk_text,
                               gint     start_pos,
                               gint     end_pos)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return NULL;

  return _gtk_entry_get_display_text (GTK_ENTRY (widget), start_pos, end_pos);
}

static gchar *
gtk_entry_accessible_get_text_before_offset (AtkText         *text,
                                             gint             offset,
                                             AtkTextBoundary  boundary_type,
                                             gint            *start_offset,
                                             gint            *end_offset)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  return _gtk_pango_get_text_before (gtk_entry_get_layout (GTK_ENTRY (widget)),
                                     boundary_type, offset,
                                     start_offset, end_offset);
}

static gchar *
gtk_entry_accessible_get_text_at_offset (AtkText         *text,
                                         gint             offset,
                                         AtkTextBoundary  boundary_type,
                                         gint            *start_offset,
                                         gint            *end_offset)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  return _gtk_pango_get_text_at (gtk_entry_get_layout (GTK_ENTRY (widget)),
                                 boundary_type, offset,
                                 start_offset, end_offset);
}

static gchar *
gtk_entry_accessible_get_text_after_offset (AtkText         *text,
                                            gint             offset,
                                            AtkTextBoundary  boundary_type,
                                            gint            *start_offset,
                                            gint            *end_offset)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  return _gtk_pango_get_text_after (gtk_entry_get_layout (GTK_ENTRY (widget)),
                                    boundary_type, offset,
                                    start_offset, end_offset);
}

static gint
gtk_entry_accessible_get_character_count (AtkText *atk_text)
{
  GtkWidget *widget;
  gchar *text;
  glong char_count;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return 0;

  text = _gtk_entry_get_display_text (GTK_ENTRY (widget), 0, -1);

  char_count = 0;
  if (text)
    {
      char_count = g_utf8_strlen (text, -1);
      g_free (text);
    }

  return char_count;
}

static gint
gtk_entry_accessible_get_caret_offset (AtkText *text)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return 0;

  return gtk_editable_get_position (GTK_EDITABLE (widget));
}

static gboolean
gtk_entry_accessible_set_caret_offset (AtkText *text,
                                       gint     offset)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  gtk_editable_set_position (GTK_EDITABLE (widget), offset);

  return TRUE;
}

static AtkAttributeSet *
add_text_attribute (AtkAttributeSet  *attributes,
                    AtkTextAttribute  attr,
                    gint              i)
{
  AtkAttribute *at;

  at = g_new (AtkAttribute, 1);
  at->name = g_strdup (atk_text_attribute_get_name (attr));
  at->value = g_strdup (atk_text_attribute_get_value (attr, i));

  return g_slist_prepend (attributes, at);
}

static AtkAttributeSet *
gtk_entry_accessible_get_run_attributes (AtkText *text,
                                         gint     offset,
                                         gint    *start_offset,
                                         gint    *end_offset)
{
  GtkWidget *widget;
  AtkAttributeSet *attributes;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  attributes = NULL;
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_DIRECTION,
                                   gtk_widget_get_direction (widget));
  attributes = _gtk_pango_get_run_attributes (attributes,
                                              gtk_entry_get_layout (GTK_ENTRY (widget)),
                                              offset,
                                              start_offset,
                                              end_offset);

  return attributes;
}

static AtkAttributeSet *
gtk_entry_accessible_get_default_attributes (AtkText *text)
{
  GtkWidget *widget;
  AtkAttributeSet *attributes;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  attributes = NULL;
  attributes = add_text_attribute (attributes, ATK_TEXT_ATTR_DIRECTION,
                                   gtk_widget_get_direction (widget));
  attributes = _gtk_pango_get_default_attributes (attributes,
                                                  gtk_entry_get_layout (GTK_ENTRY (widget)));
  attributes = _gtk_style_context_get_attributes (attributes,
                                                  gtk_widget_get_style_context (widget),
                                                  gtk_widget_get_state_flags (widget));

  return attributes;
}

static void
gtk_entry_accessible_get_character_extents (AtkText      *text,
                                            gint          offset,
                                            gint         *x,
                                            gint         *y,
                                            gint         *width,
                                            gint         *height,
                                            AtkCoordType  coords)
{
  GtkWidget *widget;
  GtkEntry *entry;
  PangoRectangle char_rect;
  gchar *entry_text;
  gint index, x_layout, y_layout;
  GdkWindow *window;
  gint x_window, y_window;
  GtkAllocation allocation;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  entry = GTK_ENTRY (widget);

  gtk_entry_get_layout_offsets (entry, &x_layout, &y_layout);
  entry_text = _gtk_entry_get_display_text (entry, 0, -1);
  index = g_utf8_offset_to_pointer (entry_text, offset) - entry_text;
  g_free (entry_text);

  pango_layout_index_to_pos (gtk_entry_get_layout (entry), index, &char_rect);
  pango_extents_to_pixels (&char_rect, NULL);

  _gtk_widget_get_allocation (widget, &allocation);

  window = gtk_widget_get_window (widget);
  gdk_window_get_origin (window, &x_window, &y_window);

  *x = x_window + allocation.x + x_layout + char_rect.x;
  *y = y_window + allocation.y + y_layout + char_rect.y;
  *width = char_rect.width;
  *height = char_rect.height;

  if (coords == ATK_XY_WINDOW)
    {
      window = gdk_window_get_toplevel (window);
      gdk_window_get_origin (window, &x_window, &y_window);

      *x -= x_window;
      *y -= y_window;
    }
}

static gint
gtk_entry_accessible_get_offset_at_point (AtkText      *atk_text,
                                          gint          x,
                                          gint          y,
                                          AtkCoordType  coords)
{
  GtkWidget *widget;
  GtkEntry *entry;
  gchar *text;
  gint index, x_layout, y_layout;
  gint x_window, y_window;
  gint x_local, y_local;
  GdkWindow *window;
  glong offset;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return -1;

  entry = GTK_ENTRY (widget);

  gtk_entry_get_layout_offsets (entry, &x_layout, &y_layout);

  window = gtk_widget_get_window (widget);
  gdk_window_get_origin (window, &x_window, &y_window);

  x_local = x - x_layout - x_window;
  y_local = y - y_layout - y_window;

  if (coords == ATK_XY_WINDOW)
    {
      window = gdk_window_get_toplevel (window);
      gdk_window_get_origin (window, &x_window, &y_window);

      x_local += x_window;
      y_local += y_window;
    }
  if (!pango_layout_xy_to_index (gtk_entry_get_layout (entry),
                                 x_local * PANGO_SCALE,
                                 y_local * PANGO_SCALE,
                                 &index, NULL))
    {
      if (x_local < 0 || y_local < 0)
        index = 0;
      else
        index = -1;
    }

  offset = -1;
  if (index != -1)
    {
      text = _gtk_entry_get_display_text (entry, 0, -1);
      offset = g_utf8_pointer_to_offset (text, text + index);
      g_free (text);
    }

  return offset;
}

static gint
gtk_entry_accessible_get_n_selections (AtkText *text)
{
  GtkWidget *widget;
  gint start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return 0;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), &start, &end))
    return 1;

  return 0;
}

static gchar *
gtk_entry_accessible_get_selection (AtkText *text,
                                    gint     selection_num,
                                    gint    *start_pos,
                                    gint    *end_pos)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return NULL;

  if (selection_num != 0)
     return NULL;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), start_pos, end_pos))
    return gtk_editable_get_chars (GTK_EDITABLE (widget), *start_pos, *end_pos);

  return NULL;
}

static gboolean
gtk_entry_accessible_add_selection (AtkText *text,
                                    gint     start_pos,
                                    gint     end_pos)
{
  GtkEntry *entry;
  GtkWidget *widget;
  gint start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  entry = GTK_ENTRY (widget);

  if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end))
    {
      gtk_editable_select_region (GTK_EDITABLE (entry), start_pos, end_pos);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_entry_accessible_remove_selection (AtkText *text,
                                       gint     selection_num)
{
  GtkWidget *widget;
  gint start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  if (selection_num != 0)
     return FALSE;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), &start, &end))
    {
      gtk_editable_select_region (GTK_EDITABLE (widget), end, end);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_entry_accessible_set_selection (AtkText *text,
                                    gint     selection_num,
                                    gint     start_pos,
                                    gint     end_pos)
{
  GtkWidget *widget;
  gint start, end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return FALSE;

  if (selection_num != 0)
     return FALSE;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), &start, &end))
    {
      gtk_editable_select_region (GTK_EDITABLE (widget), start_pos, end_pos);
      return TRUE;
    }
  else
    return FALSE;
}

static gunichar
gtk_entry_accessible_get_character_at_offset (AtkText *atk_text,
                                              gint     offset)
{
  GtkWidget *widget;
  gchar *text;
  gchar *index;
  gunichar result;

  result = '\0';

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_text));
  if (widget == NULL)
    return result;

  if (!gtk_entry_get_visibility (GTK_ENTRY (widget)))
    return result;

  text = _gtk_entry_get_display_text (GTK_ENTRY (widget), 0, -1);
  if (offset < g_utf8_strlen (text, -1))
    {
      index = g_utf8_offset_to_pointer (text, offset);
      result = g_utf8_get_char (index);
      g_free (text);
    }

  return result;
}

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gtk_entry_accessible_get_text;
  iface->get_character_at_offset = gtk_entry_accessible_get_character_at_offset;
  iface->get_text_before_offset = gtk_entry_accessible_get_text_before_offset;
  iface->get_text_at_offset = gtk_entry_accessible_get_text_at_offset;
  iface->get_text_after_offset = gtk_entry_accessible_get_text_after_offset;
  iface->get_caret_offset = gtk_entry_accessible_get_caret_offset;
  iface->set_caret_offset = gtk_entry_accessible_set_caret_offset;
  iface->get_character_count = gtk_entry_accessible_get_character_count;
  iface->get_n_selections = gtk_entry_accessible_get_n_selections;
  iface->get_selection = gtk_entry_accessible_get_selection;
  iface->add_selection = gtk_entry_accessible_add_selection;
  iface->remove_selection = gtk_entry_accessible_remove_selection;
  iface->set_selection = gtk_entry_accessible_set_selection;
  iface->get_run_attributes = gtk_entry_accessible_get_run_attributes;
  iface->get_default_attributes = gtk_entry_accessible_get_default_attributes;
  iface->get_character_extents = gtk_entry_accessible_get_character_extents;
  iface->get_offset_at_point = gtk_entry_accessible_get_offset_at_point;
}

static void
gtk_entry_accessible_set_text_contents (AtkEditableText *text,
                                        const gchar     *string)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  if (!gtk_editable_get_editable (GTK_EDITABLE (widget)))
    return;

  gtk_entry_set_text (GTK_ENTRY (widget), string);
}

static void
gtk_entry_accessible_insert_text (AtkEditableText *text,
                                  const gchar     *string,
                                  gint             length,
                                  gint            *position)
{
  GtkWidget *widget;
  GtkEditable *editable;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  editable = GTK_EDITABLE (widget);
  if (!gtk_editable_get_editable (editable))
    return;

  gtk_editable_insert_text (editable, string, length, position);
  gtk_editable_set_position (editable, *position);
}

static void
gtk_entry_accessible_copy_text (AtkEditableText *text,
                                gint             start_pos,
                                gint             end_pos)
{
  GtkWidget *widget;
  GtkEditable *editable;
  gchar *str;
  GtkClipboard *clipboard;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  if (!gtk_widget_has_screen (widget))
    return;

  editable = GTK_EDITABLE (widget);
  str = gtk_editable_get_chars (editable, start_pos, end_pos);
  clipboard = gtk_widget_get_clipboard (widget, GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, str, -1);
  g_free (str);
}

static void
gtk_entry_accessible_cut_text (AtkEditableText *text,
                               gint             start_pos,
                               gint             end_pos)
{
  GtkWidget *widget;
  GtkEditable *editable;
  gchar *str;
  GtkClipboard *clipboard;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  if (!gtk_widget_has_screen (widget))
    return;

  editable = GTK_EDITABLE (widget);
  if (!gtk_editable_get_editable (editable))
    return;

  str = gtk_editable_get_chars (editable, start_pos, end_pos);
  clipboard = gtk_widget_get_clipboard (widget, GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, str, -1);
  gtk_editable_delete_text (editable, start_pos, end_pos);
}

static void
gtk_entry_accessible_delete_text (AtkEditableText *text,
                                  gint             start_pos,
                                  gint             end_pos)
{
  GtkWidget *widget;
  GtkEditable *editable;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  editable = GTK_EDITABLE (widget);
  if (!gtk_editable_get_editable (editable))
    return;

  gtk_editable_delete_text (editable, start_pos, end_pos);
}

typedef struct
{
  GtkEntry* entry;
  gint position;
} PasteData;

static void
paste_received_cb (GtkClipboard *clipboard,
                   const gchar  *text,
                   gpointer      data)
{
  PasteData *paste = data;

  if (text)
    gtk_editable_insert_text (GTK_EDITABLE (paste->entry), text, -1,
                              &paste->position);

  g_object_unref (paste->entry);
  g_free (paste);
}

static void
gtk_entry_accessible_paste_text (AtkEditableText *text,
                                 gint             position)
{
  GtkWidget *widget;
  GtkEditable *editable;
  PasteData *paste;
  GtkClipboard *clipboard;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    return;

  if (!gtk_widget_has_screen (widget))
    return;

  editable = GTK_EDITABLE (widget);
  if (!gtk_editable_get_editable (editable))
    return;

  paste = g_new0 (PasteData, 1);
  paste->entry = GTK_ENTRY (widget);
  paste->position = position;

  g_object_ref (paste->entry);
  clipboard = gtk_widget_get_clipboard (widget, GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_request_text (clipboard, paste_received_cb, paste);
}

static void
atk_editable_text_interface_init (AtkEditableTextIface *iface)
{
  iface->set_text_contents = gtk_entry_accessible_set_text_contents;
  iface->insert_text = gtk_entry_accessible_insert_text;
  iface->copy_text = gtk_entry_accessible_copy_text;
  iface->cut_text = gtk_entry_accessible_cut_text;
  iface->delete_text = gtk_entry_accessible_delete_text;
  iface->paste_text = gtk_entry_accessible_paste_text;
  iface->set_run_attributes = NULL;
}

static void
insert_text_cb (GtkEditable *editable,
                gchar       *new_text,
                gint         new_text_length,
                gint        *position)
{
  GtkEntryAccessible *accessible;
  gint length;

  if (new_text_length == 0)
    return;

  accessible = GTK_ENTRY_ACCESSIBLE (gtk_widget_get_accessible (GTK_WIDGET (editable)));
  length = g_utf8_strlen (new_text, new_text_length);

  g_signal_emit_by_name (accessible,
                         "text-changed::insert",
                         *position - length,
                          length);
}

/* We connect to GtkEditable::delete-text, since it carries
 * the information we need. But we delay emitting our own
 * text_changed::delete signal until the entry has update
 * all its internal state and emits GtkEntry::changed.
 */
static void
delete_text_cb (GtkEditable *editable,
                gint         start,
                gint         end)
{
  GtkEntryAccessible *accessible;

  accessible = GTK_ENTRY_ACCESSIBLE (gtk_widget_get_accessible (GTK_WIDGET (editable)));

  if (end < 0)
    {
      gchar *text;

      text = _gtk_entry_get_display_text (GTK_ENTRY (editable), 0, -1);
      end = g_utf8_strlen (text, -1);
      g_free (text);
    }

  if (end == start)
    return;

  g_signal_emit_by_name (accessible,
                         "text-changed::delete",
                         start,
                         end - start);
}

static gboolean
check_for_selection_change (GtkEntryAccessible *accessible,
                            GtkEntry           *entry)
{
  gboolean ret_val = FALSE;
  gint start, end;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end))
    {
      if (end != accessible->priv->cursor_position ||
          start != accessible->priv->selection_bound)
        /*
         * This check is here as this function can be called
         * for notification of selection_bound and current_pos.
         * The values of current_pos and selection_bound may be the same
         * for both notifications and we only want to generate one
         * text_selection_changed signal.
         */
        ret_val = TRUE;
    }
  else
    {
      /* We had a selection */
      ret_val = (accessible->priv->cursor_position != accessible->priv->selection_bound);
    }

  accessible->priv->cursor_position = end;
  accessible->priv->selection_bound = start;

  return ret_val;
}

static gboolean
gtk_entry_accessible_do_action (AtkAction *action,
                                gint       i)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    return FALSE;

  if (!gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  if (i != 0)
    return FALSE;

  gtk_widget_activate (widget);

  return TRUE;
}

static gint
gtk_entry_accessible_get_n_actions (AtkAction *action)
{
  return 1;
}

static const gchar *
gtk_entry_accessible_get_keybinding (AtkAction *action,
                                     gint       i)
{
  GtkWidget *widget;
  GtkWidget *label;
  AtkRelationSet *set;
  AtkRelation *relation;
  GPtrArray *target;
  gpointer target_object;
  guint key_val;

  if (i != 0)
    return NULL;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    return NULL;

  set = atk_object_ref_relation_set (ATK_OBJECT (action));
  if (!set)
    return NULL;

  label = NULL;
  relation = atk_relation_set_get_relation_by_type (set, ATK_RELATION_LABELLED_BY);
  if (relation)
    {
      target = atk_relation_get_target (relation);

      target_object = g_ptr_array_index (target, 0);
      label = gtk_accessible_get_widget (GTK_ACCESSIBLE (target_object));
    }

  g_object_unref (set);

  if (GTK_IS_LABEL (label))
    {
      key_val = gtk_label_get_mnemonic_keyval (GTK_LABEL (label));
      if (key_val != GDK_KEY_VoidSymbol)
        return gtk_accelerator_name (key_val, GDK_MOD1_MASK);
    }

  return NULL;
}

static const gchar*
gtk_entry_accessible_action_get_name (AtkAction *action,
                                      gint       i)
{
  if (i == 0)
    return "activate";
  return NULL;
}

static const gchar*
gtk_entry_accessible_action_get_localized_name (AtkAction *action,
                                                gint       i)
{
  if (i == 0)
    return C_("Action name", "Activate");
  return NULL;
}

static const gchar*
gtk_entry_accessible_action_get_description (AtkAction *action,
                                             gint       i)
{
  if (i == 0)
    return C_("Action description", "Activates the entry");
  return NULL;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_entry_accessible_do_action;
  iface->get_n_actions = gtk_entry_accessible_get_n_actions;
  iface->get_keybinding = gtk_entry_accessible_get_keybinding;
  iface->get_name = gtk_entry_accessible_action_get_name;
  iface->get_localized_name = gtk_entry_accessible_action_get_localized_name;
  iface->get_description = gtk_entry_accessible_action_get_description;
}
