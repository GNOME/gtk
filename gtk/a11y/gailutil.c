/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2011, F123 Consulting & Mais Diferenças
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

#include <stdlib.h>
#include <gtk/gtk.h>
#include "gailutil.h"
#include "gtktoplevelaccessible.h"
#include "gtkwindowaccessible.h"

static GSList *key_listener_list = NULL;

typedef struct _GailKeyEventInfo GailKeyEventInfo;

struct _GailKeyEventInfo
{
  AtkKeyEventStruct *key_event;
  gpointer func_data;
};

static gboolean
state_event_watcher (GSignalInvocationHint *hint,
                     guint                  n_param_values,
                     const GValue          *param_values,
                     gpointer               data)
{
  GObject *object;
  GtkWidget *widget;
  AtkObject *atk_obj;
  AtkObject *parent;
  GdkEventWindowState *event;
  gchar *signal_name;

  object = g_value_get_object (param_values + 0);
  if (!GTK_IS_WINDOW (object))
    return FALSE;

  event = g_value_get_boxed (param_values + 1);
  if (event->type == GDK_WINDOW_STATE)
    return FALSE;
  widget = GTK_WIDGET (object);

  if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
    signal_name = "maximize";
  else if (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED)
    signal_name = "minimize";
  else if (event->new_window_state == 0)
    signal_name = "restore";
  else
    return TRUE;

  atk_obj = gtk_widget_get_accessible (widget);
  if (GTK_IS_WINDOW_ACCESSIBLE (atk_obj))
    {
      parent = atk_object_get_parent (atk_obj);
      if (parent == atk_get_root ())
        g_signal_emit_by_name (atk_obj, signal_name);

      return TRUE;
    }

  return FALSE;
}

static gboolean
configure_event_watcher (GSignalInvocationHint *hint,
                         guint                  n_param_values,
                         const GValue          *param_values,
                         gpointer               data)
{
  GtkAllocation allocation;
  GObject *object;
  GtkWidget *widget;
  AtkObject *atk_obj;
  AtkObject *parent;
  GdkEvent *event;
  gchar *signal_name;

  object = g_value_get_object (param_values + 0);
  if (!GTK_IS_WINDOW (object))
    return FALSE;

  event = g_value_get_boxed (param_values + 1);
  if (event->type != GDK_CONFIGURE)
    return FALSE;
  widget = GTK_WIDGET (object);
  gtk_widget_get_allocation (widget, &allocation);
  if (allocation.x == ((GdkEventConfigure *)event)->x &&
      allocation.y == ((GdkEventConfigure *)event)->y &&
      allocation.width == ((GdkEventConfigure *)event)->width &&
      allocation.height == ((GdkEventConfigure *)event)->height)
    return TRUE;

  if (allocation.width != ((GdkEventConfigure *)event)->width ||
      allocation.height != ((GdkEventConfigure *)event)->height)
    signal_name = "resize";
  else
    signal_name = "move";

  atk_obj = gtk_widget_get_accessible (widget);
  if (GTK_IS_WINDOW_ACCESSIBLE (atk_obj))
    {
      parent = atk_object_get_parent (atk_obj);
      if (parent == atk_get_root ())
        g_signal_emit_by_name (atk_obj, signal_name);

      return TRUE;
    }

  return FALSE;
}

static gboolean
window_focus (GtkWidget     *widget,
              GdkEventFocus *event)
{
  AtkObject *atk_obj;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  atk_obj = gtk_widget_get_accessible (widget);
  g_signal_emit_by_name (atk_obj, event->in ? "activate" : "deactivate");

  return FALSE;
}

static void
window_added (AtkObject *atk_obj,
              guint      index,
              AtkObject *child)
{
  GtkWidget *widget;

  if (!GTK_IS_WINDOW_ACCESSIBLE (child))
    return;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (child));
  if (!widget)
    return;

  g_signal_connect (widget, "focus-in-event", (GCallback) window_focus, NULL);
  g_signal_connect (widget, "focus-out-event", (GCallback) window_focus, NULL);
  g_signal_emit_by_name (child, "create");
}


static void
window_removed (AtkObject *atk_obj,
                guint      index,
                AtkObject *child)
{
  GtkWidget *widget;
  GtkWindow *window;

  if (!GTK_IS_WINDOW_ACCESSIBLE (child))
    return;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (child));
  if (!widget)
    return;

  window = GTK_WINDOW (widget);
  /*
   * Deactivate window if it is still focused and we are removing it. This
   * can happen when a dialog displayed by gok is removed.
   */
  if (gtk_window_is_active (window) &&
      gtk_window_has_toplevel_focus (window))
    g_signal_emit_by_name (child, "deactivate");

  g_signal_handlers_disconnect_by_func (widget, (gpointer) window_focus, NULL);
  g_signal_emit_by_name (child, "destroy");
}

static void
do_window_event_initialization (void)
{
  AtkObject *root;

  g_type_class_ref (GTK_TYPE_WINDOW_ACCESSIBLE);
  g_signal_add_emission_hook (g_signal_lookup ("window-state-event", GTK_TYPE_WIDGET),
                              0, state_event_watcher, NULL, (GDestroyNotify) NULL);
  g_signal_add_emission_hook (g_signal_lookup ("configure-event", GTK_TYPE_WIDGET),
                              0, configure_event_watcher, NULL, (GDestroyNotify) NULL);

  root = atk_get_root ();
  g_signal_connect (root, "children-changed::add",
                    (GCallback) window_added, NULL);
  g_signal_connect (root, "children-changed::remove",
                    (GCallback) window_removed, NULL);
}

static void
undo_window_event_initialization (void)
{
  AtkObject *root;

  root = atk_get_root ();

  g_signal_handlers_disconnect_by_func (root, (GCallback) window_added, NULL);
  g_signal_handlers_disconnect_by_func (root, (GCallback) window_removed, NULL);
}

static AtkKeyEventStruct *
atk_key_event_from_gdk_event_key (GdkEventKey *key)
{
  AtkKeyEventStruct *event = g_new0 (AtkKeyEventStruct, 1);
  switch (key->type)
    {
    case GDK_KEY_PRESS:
            event->type = ATK_KEY_EVENT_PRESS;
            break;
    case GDK_KEY_RELEASE:
            event->type = ATK_KEY_EVENT_RELEASE;
            break;
    default:
            g_assert_not_reached ();
            return NULL;
    }
  event->state = key->state;
  event->keyval = key->keyval;
  event->length = key->length;
  if (key->string && key->string [0] &&
      (key->state & GDK_CONTROL_MASK ||
       g_unichar_isgraph (g_utf8_get_char (key->string))))
    {
      event->string = key->string;
    }
  else if (key->type == GDK_KEY_PRESS ||
           key->type == GDK_KEY_RELEASE)
    {
      event->string = gdk_keyval_name (key->keyval);
    }
  event->keycode = key->hardware_keycode;
  event->timestamp = key->time;
#ifdef GAIL_DEBUG
  g_print ("GailKey:\tsym %u\n\tmods %x\n\tcode %u\n\ttime %lx\n",
           (unsigned int) event->keyval,
           (unsigned int) event->state,
           (unsigned int) event->keycode,
           (unsigned long int) event->timestamp);
#endif
  return event;
}

typedef struct {
  AtkKeySnoopFunc func;
  gpointer        data;
  guint           key;
} KeyEventListener;

gboolean
_gail_util_key_snooper (GtkWidget   *the_widget,
                        GdkEventKey *event)
{
  GSList *l;
  AtkKeyEventStruct *atk_event;
  gboolean result;

  atk_event = atk_key_event_from_gdk_event_key (event);

  result = FALSE;

  for (l = key_listener_list; l; l = l->next)
    {
      KeyEventListener *listener = l->data;

      result |= listener->func (atk_event, listener->data);
    }
  g_free (atk_event);

  return result;
}

static guint
gail_util_add_key_event_listener (AtkKeySnoopFunc  listener_func,
                                  gpointer         listener_data)
{
  static guint key = 0;
  KeyEventListener *listener;

  key++;

  listener = g_slice_new0 (KeyEventListener);
  listener->func = listener_func;
  listener->data = listener_data;
  listener->key = key;

  key_listener_list = g_slist_append (key_listener_list, listener);

  return key;
}

static void
gail_util_remove_key_event_listener (guint listener_key)
{
  GSList *l;

  for (l = key_listener_list; l; l = l->next)
    {
      KeyEventListener *listener = l->data;

      if (listener->key == listener_key)
        {
          g_slice_free (KeyEventListener, listener);
          key_listener_list = g_slist_delete_link (key_listener_list, l);

          break;
        }
    }
}

static AtkObject *
gail_util_get_root (void)
{
  static AtkObject *root = NULL;

  if (!root)
    {
      root = g_object_new (GTK_TYPE_TOPLEVEL_ACCESSIBLE, NULL);
      atk_object_initialize (root, NULL);
    }

  return root;
}

static const gchar *
gail_util_get_toolkit_name (void)
{
  return "gtk";
}

static const gchar *
gail_util_get_toolkit_version (void)
{
  return GTK_VERSION;
}

void
_gail_util_uninstall (void)
{
  undo_window_event_initialization ();
}

void
_gail_util_install (void)
{
  AtkUtilClass *atk_class = ATK_UTIL_CLASS (g_type_class_ref (ATK_TYPE_UTIL));

  atk_class->add_key_event_listener = gail_util_add_key_event_listener;
  atk_class->remove_key_event_listener = gail_util_remove_key_event_listener;
  atk_class->get_root = gail_util_get_root;
  atk_class->get_toolkit_name = gail_util_get_toolkit_name;
  atk_class->get_toolkit_version = gail_util_get_toolkit_version;

  do_window_event_initialization ();
}
