/* GTK+ - accessibility implementations
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
#include "gtkaccessibility.h"
#include "gtkaccessibilityutil.h"
#include "gtktoplevelaccessible.h"

static GSList *key_listener_list = NULL;

typedef struct {
  AtkKeySnoopFunc func;
  gpointer        data;
  guint           key;
} KeyEventListener;

static guint
add_key_event_listener (AtkKeySnoopFunc  listener_func,
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
remove_key_event_listener (guint listener_key)
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
get_root (void)
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
get_toolkit_name (void)
{
  return "gtk";
}

static const gchar *
get_toolkit_version (void)
{
  return GTK_VERSION;
}

void
_gtk_accessibility_override_atk_util (void)
{
  AtkUtilClass *atk_class = ATK_UTIL_CLASS (g_type_class_ref (ATK_TYPE_UTIL));

  atk_class->add_key_event_listener = add_key_event_listener;
  atk_class->remove_key_event_listener = remove_key_event_listener;
  atk_class->get_root = get_root;
  atk_class->get_toolkit_name = get_toolkit_name;
  atk_class->get_toolkit_version = get_toolkit_version;
}
