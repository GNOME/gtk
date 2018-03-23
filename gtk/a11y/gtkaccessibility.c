/* GTK+ - accessibility implementations
 * Copyright 2001 Sun Microsystems Inc.
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

#include "gtkaccessibility.h"
#include "gtkaccessibilityutil.h"

#include "gtkwindowaccessible.h"

#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkaccessible.h>

#ifdef GDK_WINDOWING_X11
#include <atk-bridge.h>
#endif

static int initialized = FALSE;

static gboolean
window_focus (GtkWidget     *widget,
              GParamSpec    *pspec)
{
  AtkObject *atk_obj;
  gboolean focus_in;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);

  focus_in = gtk_window_is_active (GTK_WINDOW (widget));

  atk_obj = gtk_widget_get_accessible (widget);
  g_signal_emit_by_name (atk_obj, focus_in ? "activate" : "deactivate");

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

  g_signal_connect (widget, "notify::is-active", (GCallback) window_focus, NULL);
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
  if (gtk_window_is_active (window))
    g_signal_emit_by_name (child, "deactivate");

  g_signal_handlers_disconnect_by_func (widget, (gpointer) window_focus, NULL);
  g_signal_emit_by_name (child, "destroy");
}

static void
do_window_event_initialization (void)
{
  AtkObject *root;

  g_type_class_ref (GTK_TYPE_WINDOW_ACCESSIBLE);

  root = atk_get_root ();
  g_signal_connect (root, "children-changed::add", (GCallback) window_added, NULL);
  g_signal_connect (root, "children-changed::remove", (GCallback) window_removed, NULL);
}

void
_gtk_accessibility_init (void)
{
  if (initialized)
    return;

  initialized = TRUE;

  _gtk_accessibility_override_atk_util ();
  do_window_event_initialization ();

#ifdef GDK_WINDOWING_X11
  atk_bridge_adaptor_init (NULL, NULL);
#endif

}
