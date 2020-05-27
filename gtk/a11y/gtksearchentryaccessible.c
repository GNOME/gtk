/* gtksearchentryaccessible.c: GtkWidgetAccessible for GtkSearchEntry
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

#include "gtksearchentryaccessible.h"

#include "gtkintl.h"

struct _GtkSearchEntryAccessible
{
  GtkEntryAccessible parent_instance;
};

G_DEFINE_TYPE (GtkSearchEntryAccessible, gtk_search_entry_accessible, GTK_TYPE_ENTRY_ACCESSIBLE)

static AtkAttributeSet *
gtk_search_entry_accessible_get_attributes (AtkObject *accessible)
{
  GtkWidget *widget;
  AtkAttributeSet *attributes;
  AtkAttribute *placeholder_text;
  char *text = NULL;

  attributes = ATK_OBJECT_CLASS (gtk_search_entry_accessible_parent_class)->get_attributes (accessible);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return attributes;

  g_object_get (widget, "placeholder-text", &text, NULL);
  if (text == NULL)
    return attributes;

  placeholder_text = g_malloc (sizeof (AtkAttribute));
  placeholder_text->name = g_strdup ("placeholder-text");
  placeholder_text->value = text;

  attributes = g_slist_append (attributes, placeholder_text);

  return attributes;
}

static void
gtk_search_entry_accessible_class_init (GtkSearchEntryAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_attributes = gtk_search_entry_accessible_get_attributes;
}

static void
gtk_search_entry_accessible_init (GtkSearchEntryAccessible *self)
{
  AtkObject *atk_obj = ATK_OBJECT (self);

  atk_obj->role = ATK_ROLE_TEXT;
  atk_object_set_name (atk_obj, _("Search"));
}
