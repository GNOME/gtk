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

#include "gtkaccessible.h"
#include "gtksearchentry.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"

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
 * To make filtering appear more reactive, it is a good idea to
 * not react to every change in the entry text immediately, but
 * only after a short delay. To support this, #GtkSearchEntry
 * emits the #GtkSearchEntry::search-changed signal which can
 * be used instead of the #GtkEditable::changed signal.
 *
 * Since: 3.6
 */

enum {
  SEARCH_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
  guint delayed_changed_id;
} GtkSearchEntryPrivate;

static void gtk_search_entry_icon_release  (GtkEntry             *entry,
                                            GtkEntryIconPosition  icon_pos);
static void gtk_search_entry_changed       (GtkEditable          *editable);
static void gtk_search_entry_editable_init (GtkEditableInterface *iface);

static GtkEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GtkSearchEntry, gtk_search_entry, GTK_TYPE_ENTRY,
                         G_ADD_PRIVATE (GtkSearchEntry)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                gtk_search_entry_editable_init))

/* 150 mseconds of delay */
#define DELAYED_TIMEOUT_ID 150

/* This widget got created without a private structure, meaning
 * that we cannot now have one without breaking ABI */
#define GET_PRIV(e) ((GtkSearchEntryPrivate *) gtk_search_entry_get_instance_private ((GtkSearchEntry *) (e)))

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

  g_signal_override_class_handler ("icon-release",
                                   GTK_TYPE_SEARCH_ENTRY,
                                   G_CALLBACK (gtk_search_entry_icon_release));

  /**
   * GtkSearchEntry::search-changed:
   * @entry: the entry on which the signal was emitted
   *
   * The #GtkSearchEntry::search-changed signal is emitted with a short
   * delay of 150 milliseconds after the last change to the entry text.
   *
   * Since: 3.10
   */
  signals[SEARCH_CHANGED] =
    g_signal_new (I_("search-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, search_changed),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
gtk_search_entry_editable_init (GtkEditableInterface *iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);
  iface->do_insert_text = parent_editable_iface->do_insert_text;
  iface->do_delete_text = parent_editable_iface->do_delete_text;
  iface->insert_text = parent_editable_iface->insert_text;
  iface->delete_text = parent_editable_iface->delete_text;
  iface->get_chars = parent_editable_iface->get_chars;
  iface->set_selection_bounds = parent_editable_iface->set_selection_bounds;
  iface->get_selection_bounds = parent_editable_iface->get_selection_bounds;
  iface->set_position = parent_editable_iface->set_position;
  iface->get_position = parent_editable_iface->get_position;
  iface->changed = gtk_search_entry_changed;
}

static void
gtk_search_entry_icon_release (GtkEntry             *entry,
                               GtkEntryIconPosition  icon_pos)
{
  if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
    gtk_entry_set_text (entry, "");
}

static gboolean
gtk_search_entry_changed_timeout_cb (gpointer user_data)
{
  GtkSearchEntry *entry = user_data;
  GtkSearchEntryPrivate *priv = GET_PRIV (entry);

  g_signal_emit (entry, signals[SEARCH_CHANGED], 0);
  priv->delayed_changed_id = 0;

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
gtk_search_entry_changed (GtkEditable *editable)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (editable);
  GtkSearchEntryPrivate *priv = GET_PRIV (entry);
  const char *str, *icon_name;
  gboolean cleared;

  /* Update the icons first */
  str = gtk_entry_get_text (GTK_ENTRY (entry));

  if (str == NULL || *str == '\0')
    {
      icon_name = NULL;
      cleared = TRUE;
    }
  else
    {
      if (gtk_widget_get_direction (GTK_WIDGET (entry)) == GTK_TEXT_DIR_RTL)
        icon_name = "edit-clear-rtl-symbolic";
      else
        icon_name = "edit-clear-symbolic";
      cleared = FALSE;
    }

  g_object_set (entry,
                "secondary-icon-name", icon_name,
                "secondary-icon-activatable", !cleared,
                "secondary-icon-sensitive", !cleared,
                NULL);

  if (cleared)
    {
      if (priv->delayed_changed_id > 0)
        {
          g_source_remove (priv->delayed_changed_id);
          priv->delayed_changed_id = 0;
        }
      g_signal_emit (entry, signals[SEARCH_CHANGED], 0);
    }
  else
    {
      /* Queue up the timeout */
      reset_timeout (entry);
    }
}

static void
gtk_search_entry_init (GtkSearchEntry *entry)
{
  AtkObject *atk_obj;

  g_object_set (entry,
                "primary-icon-name", "edit-find-symbolic",
                "primary-icon-activatable", FALSE,
                "primary-icon-sensitive", FALSE,
                NULL);

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (entry));
  if (GTK_IS_ACCESSIBLE (atk_obj))
    atk_object_set_name (atk_obj, _("Search"));
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
