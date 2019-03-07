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

#include "gtksearchentryprivate.h"

#include "gtkaccessible.h"
#include "gtkbindings.h"
#include "gtkeditable.h"
#include "gtkbox.h"
#include "gtkgesturemultipress.h"
#include "gtktextprivate.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkstylecontext.h"
#include "gtkeventcontrollerkey.h"
#include "a11y/gtkentryaccessible.h"


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
 * and #GtkSearchEntry::stop-search signals can be used to implement
 * moving between search results and ending the search.
 *
 * Often, GtkSearchEntry will be fed events by means of being
 * placed inside a #GtkSearchBar. If that is not the case,
 * you can use gtk_search_entry_set_key_capture_widget() to let it
 * capture key input from another widget.
 */

enum {
  ACTIVATE,
  SEARCH_CHANGED,
  NEXT_MATCH,
  PREVIOUS_MATCH,
  STOP_SEARCH,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_PLACEHOLDER_TEXT,
  PROP_ACTIVATES_DEFAULT,
  NUM_PROPERTIES,
};

static guint signals[LAST_SIGNAL] = { 0 };

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

typedef struct {
  GtkWidget *capture_widget;
  GtkEventController *capture_widget_controller;

  GtkWidget *box;
  GtkWidget *entry;
  GtkWidget *icon;

  guint delayed_changed_id;
  gboolean content_changed;
  gboolean search_stopped;
} GtkSearchEntryPrivate;

static void gtk_search_entry_editable_init (GtkEditableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSearchEntry, gtk_search_entry, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkSearchEntry)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                gtk_search_entry_editable_init))

/* 150 mseconds of delay */
#define DELAYED_TIMEOUT_ID 150

static void
text_changed (GtkSearchEntry *entry)
{
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  priv->content_changed = TRUE;
}

static void
gtk_search_entry_finalize (GObject *object)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (object);
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  gtk_editable_finish_delegate (GTK_EDITABLE (entry));

  g_clear_pointer (&priv->box, gtk_widget_unparent);

  if (priv->delayed_changed_id > 0)
    g_source_remove (priv->delayed_changed_id);

  gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (object), NULL);

  G_OBJECT_CLASS (gtk_search_entry_parent_class)->finalize (object);
}

static void
gtk_search_entry_stop_search (GtkSearchEntry *entry)
{
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  priv->search_stopped = TRUE;
}

static void
gtk_search_entry_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (object);
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec))
    return;

  switch (prop_id)
    {
    case PROP_PLACEHOLDER_TEXT:
      gtk_text_set_placeholder_text (GTK_TEXT (priv->entry), g_value_get_string (value));
      break;

    case PROP_ACTIVATES_DEFAULT:
      gtk_text_set_activates_default (GTK_TEXT (priv->entry), g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_search_entry_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (object);
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  switch (prop_id)
    {
    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, gtk_text_get_placeholder_text (GTK_TEXT (priv->entry)));
      break;

    case PROP_ACTIVATES_DEFAULT:
      g_value_set_boolean (value, gtk_text_get_activates_default (GTK_TEXT (priv->entry)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_search_entry_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{ 
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (widget);
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  gtk_widget_measure (priv->box, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_search_entry_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{ 
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (widget);
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  gtk_widget_size_allocate (priv->box,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);
}

static AtkObject *
gtk_search_entry_get_accessible (GtkWidget *widget)
{
  AtkObject *atk_obj;

  atk_obj = GTK_WIDGET_CLASS (gtk_search_entry_parent_class)->get_accessible (widget);
  atk_object_set_name (atk_obj, _("Search"));

  return atk_obj;
}

static void
gtk_search_entry_grab_focus (GtkWidget *widget)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (widget);
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  gtk_widget_grab_focus (priv->entry);
}

static gboolean
gtk_search_entry_mnemonic_activate (GtkWidget *widget,
                                    gboolean   group_cycling)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (widget);
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  gtk_widget_grab_focus (priv->entry);

  return TRUE;
}

static void
gtk_search_entry_class_init (GtkSearchEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkBindingSet *binding_set;

  object_class->finalize = gtk_search_entry_finalize;
  object_class->get_property = gtk_search_entry_get_property;
  object_class->set_property = gtk_search_entry_set_property;

  widget_class->measure = gtk_search_entry_measure;
  widget_class->size_allocate = gtk_search_entry_size_allocate;
  widget_class->get_accessible = gtk_search_entry_get_accessible;
  widget_class->grab_focus = gtk_search_entry_grab_focus;
  widget_class->mnemonic_activate = gtk_search_entry_mnemonic_activate;

  klass->stop_search = gtk_search_entry_stop_search;

  props[PROP_PLACEHOLDER_TEXT] =
      g_param_spec_string ("placeholder-text",
                           P_("Placeholder text"),
                           P_("Show text in the entry when it’s empty and unfocused"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ACTIVATES_DEFAULT] =
      g_param_spec_boolean ("activates-default",
                            P_("Activates default"),
                            P_("Whether to activate the default widget (such as the default button in a dialog) when Enter is pressed"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);
  gtk_editable_install_properties (object_class, NUM_PROPERTIES);

  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkSearchEntry::search-changed:
   * @entry: the entry on which the signal was emitted
   *
   * The #GtkSearchEntry::search-changed signal is emitted with a short
   * delay of 150 milliseconds after the last change to the entry text.
   */
  signals[SEARCH_CHANGED] =
    g_signal_new (I_("search-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, search_changed),
                  NULL, NULL,
                  NULL,
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
   */
  signals[NEXT_MATCH] =
    g_signal_new (I_("next-match"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, next_match),
                  NULL, NULL,
                  NULL,
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
   */
  signals[PREVIOUS_MATCH] =
    g_signal_new (I_("previous-match"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, previous_match),
                  NULL, NULL,
                  NULL,
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
   */
  signals[STOP_SEARCH] =
    g_signal_new (I_("stop-search"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, stop_search),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_g, GDK_CONTROL_MASK,
                                "next-match", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_g, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                "previous-match", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0,
                                "stop-search", 0);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_ENTRY_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("entry"));
}

static GtkEditable *
gtk_search_entry_get_delegate (GtkEditable *editable)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (editable);
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  return GTK_EDITABLE (priv->entry);
}

static void
gtk_search_entry_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = gtk_search_entry_get_delegate;
}

static void
gtk_search_entry_icon_release (GtkGestureMultiPress *press,
                               int                   n_press,
                               double                x,
                               double                y,
                               GtkSearchEntry       *entry)
{
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  gtk_editable_set_text (GTK_EDITABLE (priv->entry), "");
}

static gboolean
gtk_search_entry_changed_timeout_cb (gpointer user_data)
{
  GtkSearchEntry *entry = user_data;
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  g_signal_emit (entry, signals[SEARCH_CHANGED], 0);
  priv->delayed_changed_id = 0;

  return G_SOURCE_REMOVE;
}

static void
reset_timeout (GtkSearchEntry *entry)
{
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  if (priv->delayed_changed_id > 0)
    g_source_remove (priv->delayed_changed_id);
  priv->delayed_changed_id = g_timeout_add (DELAYED_TIMEOUT_ID,
                                            gtk_search_entry_changed_timeout_cb,
                                            entry);
  g_source_set_name_by_id (priv->delayed_changed_id, "[gtk] gtk_search_entry_changed_timeout_cb");
}

static void
gtk_search_entry_changed (GtkEditable *editable,
                          GtkSearchEntry *entry)
{
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);
  const char *str;

  /* Update the icons first */
  str = gtk_editable_get_text (GTK_EDITABLE (priv->entry));

  if (str == NULL || *str == '\0')
    {
      gtk_widget_hide (priv->icon);

      if (priv->delayed_changed_id > 0)
        {
          g_source_remove (priv->delayed_changed_id);
          priv->delayed_changed_id = 0;
        }
      g_signal_emit (entry, signals[SEARCH_CHANGED], 0);
    }
  else
    {
      gtk_widget_show (priv->icon);

      /* Queue up the timeout */
      reset_timeout (entry);
    }
}

static void
notify_cb (GObject    *object,
           GParamSpec *pspec,
           gpointer    data)
{
  /* The editable interface properties are already forwarded by the editable delegate setup */
  if (g_str_equal (pspec->name, "placeholder-text") ||
      g_str_equal (pspec->name, "activates-default"))
    g_object_notify (data, pspec->name);
}

static void
activate_cb (GtkText  *text,
             gpointer  data)
{
  g_signal_emit (data, signals[ACTIVATE], 0);
}

static void
gtk_search_entry_init (GtkSearchEntry *entry)
{
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);
  GtkGesture *press;

  gtk_widget_set_has_surface (GTK_WIDGET (entry), FALSE);

  priv->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_parent (priv->box, GTK_WIDGET (entry));
  gtk_widget_set_hexpand (priv->box, FALSE);
  gtk_widget_set_vexpand (priv->box, FALSE);

  priv->entry = gtk_text_new ();
  g_object_set (priv->entry, "select-on-focus", FALSE, NULL);
  gtk_widget_set_hexpand (priv->entry, TRUE);
  gtk_widget_set_vexpand (priv->entry, TRUE);
  gtk_container_add (GTK_CONTAINER (priv->box), GTK_WIDGET (priv->entry));
  gtk_editable_init_delegate (GTK_EDITABLE (entry));
  g_signal_connect_swapped (priv->entry, "changed", G_CALLBACK (text_changed), entry);
  g_signal_connect_after (priv->entry, "changed", G_CALLBACK (gtk_search_entry_changed), entry);
  g_signal_connect_swapped (priv->entry, "preedit-changed", G_CALLBACK (text_changed), entry);
  g_signal_connect (priv->entry, "notify", G_CALLBACK (notify_cb), entry);
  g_signal_connect (priv->entry, "activate", G_CALLBACK (activate_cb), entry);

  priv->icon = gtk_image_new_from_icon_name ("edit-clear-symbolic");
  gtk_widget_set_tooltip_text (priv->icon, _("Clear entry"));
  gtk_container_add (GTK_CONTAINER (priv->box), GTK_WIDGET (priv->icon));
  gtk_widget_hide (priv->icon);

  press = gtk_gesture_multi_press_new ();
  g_signal_connect (press, "released", G_CALLBACK (gtk_search_entry_icon_release), entry);
  gtk_widget_add_controller (priv->icon, GTK_EVENT_CONTROLLER (press));

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (entry)), I_("search"));
}

/**
 * gtk_search_entry_new:
 *
 * Creates a #GtkSearchEntry, with a find icon when the search field is
 * empty, and a clear icon when it isn't.
 *
 * Returns: a new #GtkSearchEntry
 */
GtkWidget *
gtk_search_entry_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_SEARCH_ENTRY, NULL));
}

gboolean
gtk_search_entry_is_keynav (guint           keyval,
                            GdkModifierType state)
{
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
 */
gboolean
gtk_search_entry_handle_event (GtkSearchEntry *entry,
                               GdkEvent       *event)
{
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);
  gboolean handled;
  guint keyval, state;

  if (!gtk_widget_get_realized (GTK_WIDGET (entry)))
    gtk_widget_realize (GTK_WIDGET (entry));

  gdk_event_get_keyval (event, &keyval);
  gdk_event_get_state (event, &state);

  if (gtk_search_entry_is_keynav (keyval, state) ||
      keyval == GDK_KEY_space ||
      keyval == GDK_KEY_Menu)
    return GDK_EVENT_PROPAGATE;

  priv->content_changed = FALSE;
  priv->search_stopped = FALSE;

  handled = gtk_widget_event (GTK_WIDGET (entry), event);

  return handled && priv->content_changed && !priv->search_stopped ? GDK_EVENT_STOP : GDK_EVENT_PROPAGATE;
}

static gboolean
capture_widget_key_handled (GtkEventControllerKey *controller,
                            guint                  keyval,
                            guint                  keycode,
                            GdkModifierType        state,
                            GtkWidget             *entry)
{
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (GTK_SEARCH_ENTRY (entry));
  gboolean handled;

  if (gtk_search_entry_is_keynav (keyval, state) ||
      keyval == GDK_KEY_space ||
      keyval == GDK_KEY_Menu)
    return FALSE;

  priv->content_changed = FALSE;
  priv->search_stopped = FALSE;

  handled = gtk_event_controller_key_forward (controller, priv->entry);

  return handled && priv->content_changed && !priv->search_stopped ? GDK_EVENT_STOP : GDK_EVENT_PROPAGATE;
}

/**
 * gtk_search_entry_set_key_capture_widget:
 * @entry: a #GtkSearchEntry
 * @widget: (nullable) (transfer none): a #GtkWidget
 *
 * Sets @widget as the widget that @entry will capture key events from.
 *
 * Key events are consumed by the search entry to start or
 * continue a search.
 *
 * If the entry is part of a #GtkSearchBar, it is preferable
 * to call gtk_search_bar_set_key_capture_widget() instead, which
 * will reveal the entry in addition to triggering the search entry.
 **/
void
gtk_search_entry_set_key_capture_widget (GtkSearchEntry *entry,
                                         GtkWidget      *widget)
{
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  g_return_if_fail (GTK_IS_SEARCH_ENTRY (entry));
  g_return_if_fail (!widget || GTK_IS_WIDGET (widget));

  if (priv->capture_widget)
    {
      gtk_widget_remove_controller (priv->capture_widget,
                                    priv->capture_widget_controller);
      g_object_remove_weak_pointer (G_OBJECT (priv->capture_widget),
                                    (gpointer *) &priv->capture_widget);
    }

  priv->capture_widget = widget;

  if (widget)
    {
      g_object_add_weak_pointer (G_OBJECT (priv->capture_widget),
                                 (gpointer *) &priv->capture_widget);

      priv->capture_widget_controller = gtk_event_controller_key_new ();
      gtk_event_controller_set_propagation_phase (priv->capture_widget_controller,
                                                  GTK_PHASE_CAPTURE);
      g_signal_connect (priv->capture_widget_controller, "key-pressed",
                        G_CALLBACK (capture_widget_key_handled), entry);
      g_signal_connect (priv->capture_widget_controller, "key-released",
                        G_CALLBACK (capture_widget_key_handled), entry);
      gtk_widget_add_controller (widget, priv->capture_widget_controller);
    }
}

/**
 * gtk_search_entry_get_key_capture_widget:
 * @entry: a #GtkSearchEntry
 *
 * Gets the widget that @entry is capturing key events from.
 *
 * Returns: (transfer none): The key capture widget.
 **/
GtkWidget *
gtk_search_entry_get_key_capture_widget (GtkSearchEntry *entry)
{
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  g_return_val_if_fail (GTK_IS_SEARCH_ENTRY (entry), NULL);

  return priv->capture_widget;
}

GtkEventController *
gtk_search_entry_get_key_controller (GtkSearchEntry *entry)
{
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  return gtk_text_get_key_controller (GTK_TEXT (priv->entry));
}

GtkText *
gtk_search_entry_get_text_widget (GtkSearchEntry *entry)
{
  GtkSearchEntryPrivate *priv = gtk_search_entry_get_instance_private (entry);

  return GTK_TEXT (priv->entry);
}
