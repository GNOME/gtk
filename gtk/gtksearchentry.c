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
#include "gtkbindings.h"

/**
 * SECTION:gtksearchentry
 * @Short_description: An entry which shows a search icon
 * @Title: GtkSearchEntry
 *
 * #GtkSearchEntry is a subclass of #GtkEntry that has been
 * tailored for use as a search entry.
 *
 * It will show an inactive symbolic “find” icon when the search
 * entry is empty, and a symbolic “clear” icon when there is text.
 * Clicking on the “clear” icon will empty the search entry.
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
 * The #GtkSearchEntry::previous-match, #GtkSearchEntry::next-match
 * and #GtkSearchEntry::stop-search signals can be uesd to implement
 * moving between search results and ending the search.
 *
 * Often, GtkSearchEntry will be fed events by means of being
 * placed inside a #GtkSearchBar. If that is not the case,
 * you can use gtk_search_entry_handle_event() to pass events.
 *
 * Since: 3.6
 */

enum {
  SEARCH_CHANGED,
  NEXT_MATCH,
  PREVIOUS_MATCH,
  STOP_SEARCH,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
  guint delayed_changed_id;
  gboolean content_changed;
  gboolean search_stopped;
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
gtk_search_entry_preedit_changed (GtkEntry    *entry,
                                  const gchar *preedit)
{
  GtkSearchEntryPrivate *priv = GET_PRIV (entry);

  priv->content_changed = TRUE;
}

static void
gtk_search_entry_notify (GObject    *object,
                         GParamSpec *pspec)
{
  GtkSearchEntryPrivate *priv = GET_PRIV (object);

  if (strcmp (pspec->name, "text") == 0)
    priv->content_changed = TRUE;

  if (G_OBJECT_CLASS (gtk_search_entry_parent_class)->notify)
    G_OBJECT_CLASS (gtk_search_entry_parent_class)->notify (object, pspec);
}

static void
gtk_search_entry_finalize (GObject *object)
{
  GtkSearchEntryPrivate *priv = GET_PRIV (object);

  if (priv->delayed_changed_id > 0)
    g_source_remove (priv->delayed_changed_id);

  G_OBJECT_CLASS (gtk_search_entry_parent_class)->finalize (object);
}

static void
gtk_search_entry_stop_search (GtkSearchEntry *entry)
{
  GtkSearchEntryPrivate *priv = GET_PRIV (entry);

  priv->search_stopped = TRUE;
}

static void
gtk_search_entry_class_init (GtkSearchEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set;

  object_class->finalize = gtk_search_entry_finalize;
  object_class->notify = gtk_search_entry_notify;

  klass->stop_search = gtk_search_entry_stop_search;

  g_signal_override_class_handler ("icon-release",
                                   GTK_TYPE_SEARCH_ENTRY,
                                   G_CALLBACK (gtk_search_entry_icon_release));

  g_signal_override_class_handler ("preedit-changed",
                                   GTK_TYPE_SEARCH_ENTRY,
                                   G_CALLBACK (gtk_search_entry_preedit_changed));

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

  /**
   * GtkSearchEntry::next-match:
   * @entry: the entry on which the signal was emitted
   *
   * The ::next-match signal is a [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user initiates a move to the next match
   * for the current search string.
   *
   * Applications should connect to it, to implement moving between
   * matches.
   *
   * The default bindings for this signal is Ctrl-g.
   *
   * Since: 3.16
   */
  signals[NEXT_MATCH] =
    g_signal_new (I_("next-match"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, next_match),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkSearchEntry::previous-match:
   * @entry: the entry on which the signal was emitted
   *
   * The ::previous-match signal is a [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user initiates a move to the previous match
   * for the current search string.
   *
   * Applications should connect to it, to implement moving between
   * matches.
   *
   * The default bindings for this signal is Ctrl-Shift-g.
   *
   * Since: 3.16
   */
  signals[PREVIOUS_MATCH] =
    g_signal_new (I_("previous-match"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, previous_match),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkSearchEntry::stop-search:
   * @entry: the entry on which the signal was emitted
   *
   * The ::stop-search signal is a [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user stops a search via keyboard input.
   *
   * Applications should connect to it, to implement hiding the search
   * entry in this case.
   *
   * The default bindings for this signal is Escape.
   *
   * Since: 3.16
   */
  signals[STOP_SEARCH] =
    g_signal_new (I_("stop-search"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, stop_search),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_g, GDK_CONTROL_MASK,
                                "next-match", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_g, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                "previous-match", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0,
                                "stop-search", 0);
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
  g_source_set_name_by_id (priv->delayed_changed_id, "[gtk+] gtk_search_entry_changed_timeout_cb");
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
 * Returns: a new #GtkSearchEntry
 *
 * Since: 3.6
 */
GtkWidget *
gtk_search_entry_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_SEARCH_ENTRY, NULL));
}

gboolean
gtk_search_entry_is_keynav_event (GdkEvent *event)
{
  GdkModifierType state = 0;
  guint keyval;

  if (!gdk_event_get_keyval (event, &keyval))
    return FALSE;

  gdk_event_get_state (event, &state);

  if (keyval == GDK_KEY_Tab       || keyval == GDK_KEY_KP_Tab ||
      keyval == GDK_KEY_Up        || keyval == GDK_KEY_KP_Up ||
      keyval == GDK_KEY_Down      || keyval == GDK_KEY_KP_Down ||
      keyval == GDK_KEY_Left      || keyval == GDK_KEY_KP_Left ||
      keyval == GDK_KEY_Right     || keyval == GDK_KEY_KP_Right ||
      keyval == GDK_KEY_Home      || keyval == GDK_KEY_KP_Home ||
      keyval == GDK_KEY_End       || keyval == GDK_KEY_KP_End ||
      keyval == GDK_KEY_Page_Up   || keyval == GDK_KEY_KP_Page_Up ||
      keyval == GDK_KEY_Page_Down || keyval == GDK_KEY_KP_Page_Down ||
      ((state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) != 0))
        return TRUE;

  /* Other navigation events should get automatically
   * ignored as they will not change the content of the entry
   */
  return FALSE;
}

/**
 * gtk_search_entry_handle_event:
 * @entry: a #GtkSearchEntry
 * @event: a key event
 *
 * This function should be called when the top-level window
 * which contains the search entry received a key event. If
 * the entry is part of a #GtkSearchBar, it is preferable
 * to call gtk_search_bar_handle_event() instead, which will
 * reveal the entry in addition to passing the event to this
 * function.
 *
 * If the key event is handled by the search entry and starts
 * or continues a search, %GDK_EVENT_STOP will be returned.
 * The caller should ensure that the entry is shown in this
 * case, and not propagate the event further.
 *
 * Returns: %GDK_EVENT_STOP if the key press event resulted
 *     in a search beginning or continuing, %GDK_EVENT_PROPAGATE
 *     otherwise.
 *
 * Since: 3.16
 */
gboolean
gtk_search_entry_handle_event (GtkSearchEntry *entry,
                               GdkEvent       *event)
{
  GtkSearchEntryPrivate *priv = GET_PRIV (entry);
  gboolean handled;

  if (!gtk_widget_get_realized (GTK_WIDGET (entry)))
    gtk_widget_realize (GTK_WIDGET (entry));

  if (gtk_search_entry_is_keynav_event (event) ||
      event->key.keyval == GDK_KEY_space ||
      event->key.keyval == GDK_KEY_Menu)
    return GDK_EVENT_PROPAGATE;

  priv->content_changed = FALSE;
  priv->search_stopped = FALSE;

  handled = gtk_widget_event (GTK_WIDGET (entry), event);

  return handled && priv->content_changed && !priv->search_stopped ? GDK_EVENT_STOP : GDK_EVENT_PROPAGATE;
}
