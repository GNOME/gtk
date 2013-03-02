/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Authors:
 * - Bastien Nocera <bnocera@redhat.com>
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

/*
 * Modified by the GTK+ Team and others 2012.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtksearchentry.h"

/**
 * SECTION:gtksearchentry
 * @Short_description: An entry which shows a search icon
 * @Title: GtkSearchEntry
 *
 * #GtkSearchEntry is a subclass of #GtkEntry that has
 * been tailored for use as a search entry.
 *
 * It will show an inactive symbolic "find" icon when the
 * search entry is empty, and a symbolic "clear" icon when
 * there is text. Clicking on the "clear" icon will empty
 * the search entry.
 *
 * Note that the search/clear icon is shown using a secondary
 * icon, and thus does not work if you are using the secondary
 * icon position for some other purpose.
 *
 * Since: 3.6
 */

G_DEFINE_TYPE (GtkSearchEntry, gtk_search_entry, GTK_TYPE_ENTRY)

static void
gtk_search_entry_class_init (GtkSearchEntryClass *klass)
{
}

static void
search_entry_clear_cb (GtkEntry *entry,
                       gpointer  user_data)
{
  gtk_entry_set_text (entry, "");
}

static void
search_entry_changed_cb (GtkEntry *entry,
                         gpointer  user_data)
{
  const char *str, *icon_name;
  gboolean active;

  str = gtk_entry_get_text (entry);

  if (str == NULL || *str == '\0')
    {
      icon_name = NULL;
      active = FALSE;
    }
  else
    {
      if (gtk_widget_get_direction (GTK_WIDGET (entry)) == GTK_TEXT_DIR_RTL)
        icon_name = "edit-clear-rtl-symbolic";
      else
        icon_name = "edit-clear-symbolic";
      active = TRUE;
    }

  g_object_set (entry,
                "secondary-icon-name", icon_name,
                "secondary-icon-activatable", active,
                "secondary-icon-sensitive", active,
                NULL);
}

static void
gtk_search_entry_init (GtkSearchEntry *entry)
{
  g_signal_connect (entry, "changed",
                    G_CALLBACK (search_entry_changed_cb), NULL);
  g_signal_connect (entry, "icon-release",
                    G_CALLBACK (search_entry_clear_cb), NULL);

  g_object_set (entry,
                "primary-icon-name", "edit-find-symbolic",
                "primary-icon-activatable", FALSE,
                "primary-icon-sensitive", FALSE,
                NULL);

  search_entry_changed_cb (GTK_ENTRY (entry), NULL);
}

/**
 * gtk_search_entry_new:
 *
 * Creates a #GtkSearchEntry, with a find icon when the search field is
 * empty, and a clear icon when it isn't.
 *
 * Return value: a new #GtkSearchEntry
 *
 * Since: 3.6
 */
GtkWidget *
gtk_search_entry_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_SEARCH_ENTRY, NULL));
}
