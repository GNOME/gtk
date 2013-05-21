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

typedef struct {
  guint delayed_changed_id;
  gboolean in_timeout;
} GtkSearchEntryPrivate;

/* 150 mseconds of delay */
#define DELAYED_TIMEOUT_ID 150

/* This widget got created without a private structure, meaning
 * that we cannot now have one without breaking ABI */
#define GET_PRIV(e) G_TYPE_INSTANCE_GET_PRIVATE (e, GTK_TYPE_SEARCH_ENTRY, GtkSearchEntryPrivate)

static void
gtk_search_entry_finalize (GObject *object)
{
  GtkSearchEntryPrivate *priv = GET_PRIV (object);

  if (priv->delayed_changed_id > 0)
    g_source_remove (priv->delayed_changed_id);

  G_OBJECT_CLASS (gtk_search_entry_parent_class)->finalize (object);
}

static void
gtk_search_entry_class_init (GtkSearchEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_search_entry_finalize;

  g_type_class_add_private (klass, sizeof (GtkSearchEntryPrivate));
}

static void
search_entry_clear_cb (GtkEntry *entry,
                       gpointer  user_data)
{
  gtk_entry_set_text (entry, "");
}

static gboolean
gtk_search_entry_changed_timeout_cb (gpointer user_data)
{
  GtkSearchEntry *entry = user_data;
  GtkSearchEntryPrivate *priv = GET_PRIV (entry);

  priv->in_timeout = TRUE;
  g_signal_emit_by_name (entry, "changed");
  priv->delayed_changed_id = 0;
  priv->in_timeout = FALSE;

  return G_SOURCE_REMOVE;
}

static void
reset_timeout (GtkSearchEntry *entry)
{
  GtkSearchEntryPrivate *priv = GET_PRIV (entry);

  if (priv->delayed_changed_id > 0)
    g_source_remove (priv->delayed_changed_id);
  priv->delayed_changed_id = g_timeout_add (DELAYED_TIMEOUT_ID,
                                            gtk_search_entry_changed_timeout_cb,
                                            entry);
}

static void
search_entry_changed_cb (GtkSearchEntry *entry,
                         gpointer        user_data)
{
  GtkSearchEntryPrivate *priv = GET_PRIV (entry);
  const char *str, *icon_name;
  gboolean active;
  gboolean cleared = FALSE;

  /* Update the icons first */
  str = gtk_entry_get_text (GTK_ENTRY (entry));

  if (str == NULL || *str == '\0')
    {
      icon_name = NULL;
      active = FALSE;
      cleared = TRUE;
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

  /* Don't stop the emission if it's the timeout
   * emitting the signal, otherwise we'll get in a loop */
  if (priv->in_timeout)
    return;

  /* Don't emit the signal in a timeout if we've cleared
   * the entry, we don't want a delay */
  if (cleared)
    return;

  /* Queue up the timeout */
  reset_timeout (entry);
  g_signal_stop_emission_by_name (entry, "changed");
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
