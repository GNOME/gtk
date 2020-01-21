/* ATK -  Accessibility Toolkit
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "atkutil.h"
#include "atkmarshal.h"

/**
 * SECTION:atkutil
 * @Short_description: A set of ATK utility functions for event and toolkit support.
 * @Title:AtkUtil
 *
 * A set of ATK utility functions which are used to support event
 * registration of various types, and obtaining the 'root' accessible
 * of a process and information about the current ATK implementation
 * and toolkit version.
 */

static void atk_util_class_init (AtkUtilClass *klass);

static AtkObject *previous_focus_object = NULL;

typedef struct _AtkUtilListenerInfo AtkUtilListenerInfo;
struct _AtkUtilListenerInfo
{
  gint key;
  guint signal_id;
  gulong hook_id;
};
static GHashTable *listener_list = NULL;

GType
atk_util_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo typeInfo =
      {
        sizeof (AtkUtilClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) atk_util_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        sizeof (AtkUtil),
        0,
        (GInstanceInitFunc) NULL,
      } ;
      type = g_type_register_static (G_TYPE_OBJECT, "AtkUtil", &typeInfo, 0) ;
    }
  return type;
}

typedef struct _FocusTracker FocusTracker;

struct _FocusTracker {
  guint index;
  AtkEventListener func;
};

static guint
add_listener (GSignalEmissionHook listener,
              const gchar         *object_type,
              const gchar         *signal_name,
              const gchar         *detail_string,
              const gchar         *hook_data)
{
  GType type;
  guint signal_id;
  gint  rc = 0;
  static gint listener_idx = 1;
  GQuark detail_quark = 0;

  type = g_type_from_name (object_type);
  if (type)
    {
      signal_id  = g_signal_lookup (signal_name, type);
      detail_quark = g_quark_from_string (detail_string);

      if (signal_id > 0)
        {
          AtkUtilListenerInfo *listener_info;

          rc = listener_idx;

          listener_info = g_new (AtkUtilListenerInfo, 1);
          listener_info->key = listener_idx;
          listener_info->hook_id =
            g_signal_add_emission_hook (signal_id, detail_quark, listener,
                                        g_strdup (hook_data),
                                        (GDestroyNotify) g_free);
          listener_info->signal_id = signal_id;

	  g_hash_table_insert(listener_list, &(listener_info->key), listener_info);
          listener_idx++;
        }
      else
        {
          g_debug ("Signal type %s not supported\n", signal_name);
        }
    }
  else
    {
      g_warning("Invalid object type %s\n", object_type);
    }
  return rc;
}

static guint
atk_util_real_add_global_event_listener (GSignalEmissionHook listener,
                                         const gchar *event_type)
{
  guint rc = 0;
  gchar **split_string;
  guint length;

  split_string = g_strsplit (event_type, ":", 0);
  length = g_strv_length (split_string);

  if ((length == 3) || (length == 4))
    rc = add_listener (listener, split_string[1], split_string[2],
                       split_string[3], event_type);

  g_strfreev (split_string);

  return rc;
}

static void
atk_util_real_remove_global_event_listener (guint remove_listener)
{
  if (remove_listener > 0)
    {
      AtkUtilListenerInfo *listener_info;
      gint tmp_idx = remove_listener;

      listener_info = (AtkUtilListenerInfo *)
        g_hash_table_lookup(listener_list, &tmp_idx);

      if (listener_info != NULL)
        {
          /* Hook id of 0 and signal id of 0 are invalid */
          if (listener_info->hook_id != 0 && listener_info->signal_id != 0)
            {
              /* Remove the emission hook */
              g_signal_remove_emission_hook(listener_info->signal_id,
                                            listener_info->hook_id);

              /* Remove the element from the hash */
              g_hash_table_remove(listener_list, &tmp_idx);
            }
          else
            {
              g_warning("Invalid listener hook_id %ld or signal_id %d\n",
                        listener_info->hook_id, listener_info->signal_id);
            }
        }
      else
        {
          g_warning("No listener with the specified listener id %d",
                    remove_listener);
        }
    }
  else
    {
      g_warning("Invalid listener_id %d", remove_listener);
    }
}


/**
 * atk_add_global_event_listener: (skip)
 * @listener: the listener to notify
 * @event_type: the type of event for which notification is requested
 *
 * Adds the specified function to the list of functions to be called
 * when an ATK event of type event_type occurs.
 *
 * The format of event_type is the following:
 *  "ATK:&lt;atk_type&gt;:&lt;atk_event&gt;:&lt;atk_event_detail&gt;
 *
 * Where "ATK" works as the namespace, &lt;atk_interface&gt; is the name of
 * the ATK type (interface or object), &lt;atk_event&gt; is the name of the
 * signal defined on that interface and &lt;atk_event_detail&gt; is the
 * gsignal detail of that signal. You can find more info about gsignal
 * details here:
 * http://developer.gnome.org/gobject/stable/gobject-Signals.html
 *
 * The first three parameters are mandatory. The last one is optional.
 *
 * For example:
 *   ATK:AtkObject:state-change
 *   ATK:AtkText:text-selection-changed
 *   ATK:AtkText:text-insert:system
 *
 * Toolkit implementor note: ATK provides a default implementation for
 * this virtual method. ATK implementors are discouraged from
 * reimplementing this method.
 *
 * Toolkit implementor note: this method is not intended to be used by
 * ATK implementors but by ATK consumers.
 *
 * ATK consumers note: as this method adds a listener for a given ATK
 * type, that type should be already registered on the GType system
 * before calling this method. A simple way to do that is creating an
 * instance of #AtkNoOpObject. This class implements all ATK
 * interfaces, so creating the instance will register all ATK types as
 * a collateral effect.
 *
 * Returns: added event listener id, or 0 on failure.
 **/
guint
atk_add_global_event_listener (GSignalEmissionHook listener,
			       const gchar        *event_type)
{
  guint retval;
  AtkUtilClass *klass = g_type_class_ref (ATK_TYPE_UTIL);

  if (klass->add_global_event_listener)
    {
      retval = klass->add_global_event_listener (listener, event_type);
    }
  else
    {
      retval = 0;
    }
  g_type_class_unref (klass);

  return retval;
}

/**
 * atk_remove_global_event_listener:
 * @listener_id: the id of the event listener to remove
 *
 * @listener_id is the value returned by #atk_add_global_event_listener
 * when you registered that event listener.
 *
 * Toolkit implementor note: ATK provides a default implementation for
 * this virtual method. ATK implementors are discouraged from
 * reimplementing this method.
 *
 * Toolkit implementor note: this method is not intended to be used by
 * ATK implementors but by ATK consumers.
 *
 * Removes the specified event listener
 **/
void
atk_remove_global_event_listener (guint listener_id)
{
  AtkUtilClass *klass = g_type_class_peek (ATK_TYPE_UTIL);

  if (klass && klass->remove_global_event_listener)
    klass->remove_global_event_listener (listener_id);
}

/**
 * atk_add_key_event_listener: (skip)
 * @listener: the listener to notify
 * @data: a #gpointer that points to a block of data that should be sent to the registered listeners,
 *        along with the event notification, when it occurs.  
 *
 * Adds the specified function to the list of functions to be called
 *        when a key event occurs.  The @data element will be passed to the
 *        #AtkKeySnoopFunc (@listener) as the @func_data param, on notification.
 *
 * Returns: added event listener id, or 0 on failure.
 **/
guint
atk_add_key_event_listener (AtkKeySnoopFunc listener, gpointer data)
{
  guint retval;
  AtkUtilClass *klass = g_type_class_peek (ATK_TYPE_UTIL);
  if (klass && klass->add_key_event_listener)
    {
      retval = klass->add_key_event_listener (listener, data);
    }
  else
    {
      retval = 0;
    }

  return retval;
}

/**
 * atk_remove_key_event_listener:
 * @listener_id: the id of the event listener to remove
 *
 * @listener_id is the value returned by #atk_add_key_event_listener
 * when you registered that event listener.
 *
 * Removes the specified event listener.
 **/
void
atk_remove_key_event_listener (guint listener_id)
{
  AtkUtilClass *klass = g_type_class_peek (ATK_TYPE_UTIL);

  if (klass->remove_key_event_listener)
    klass->remove_key_event_listener (listener_id);
}

/**
 * atk_get_root:
 *
 * Gets the root accessible container for the current application.
 *
 * Returns: (transfer none): the root accessible container for the current
 * application
 **/
AtkObject*
atk_get_root (void)
{
  AtkUtilClass *klass = g_type_class_ref (ATK_TYPE_UTIL);
  AtkObject    *retval;
  if (klass->get_root)
    {
      retval = klass->get_root ();
    }
  else
    {
      retval = NULL;
    }
  g_type_class_unref (klass);

  return retval;
}

/**
 * atk_get_focus_object:
 *
 * Gets the currently focused object.
 * 
 * Since: 1.6
 *
 * Returns: (transfer none): the currently focused object for the current
 * application
 **/
AtkObject*
atk_get_focus_object (void)
{
  return previous_focus_object;
}

static void
atk_util_class_init (AtkUtilClass *klass)
{
  klass->add_global_event_listener = atk_util_real_add_global_event_listener;
  klass->remove_global_event_listener = atk_util_real_remove_global_event_listener;
  klass->get_root = NULL;

  listener_list = g_hash_table_new_full (g_int_hash, g_int_equal, NULL,
                                         g_free);
}
