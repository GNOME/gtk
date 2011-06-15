/* GAIL - The GNOME Accessibility Implementation Library
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gailutil.h"
#include "gailtoplevel.h"
#include "gailwindow.h"
#include "gail-private-macros.h"

static void		gail_util_class_init			(GailUtilClass		*klass);
static void             gail_util_init                          (GailUtil               *utils);
/* atkutil.h */

static guint		gail_util_add_global_event_listener	(GSignalEmissionHook	listener,
							         const gchar*           event_type);
static void 		gail_util_remove_global_event_listener	(guint			remove_listener);
static guint		gail_util_add_key_event_listener	(AtkKeySnoopFunc	listener,
								 gpointer               data);
static void 		gail_util_remove_key_event_listener	(guint			remove_listener);
static AtkObject*	gail_util_get_root			(void);
static const gchar *    gail_util_get_toolkit_name		(void);
static const gchar *    gail_util_get_toolkit_version      (void);

/* gailmisc/AtkMisc */
static void		gail_misc_class_init			(GailMiscClass		*klass);
static void             gail_misc_init                          (GailMisc               *misc);

static void gail_misc_threads_enter (AtkMisc *misc);
static void gail_misc_threads_leave (AtkMisc *misc);

/* Misc */

static void		_listener_info_destroy			(gpointer		data);
static guint            add_listener	                        (GSignalEmissionHook    listener,
                                                                 const gchar            *object_type,
                                                                 const gchar            *signal,
                                                                 const gchar            *hook_data);
static void             do_window_event_initialization          (void);
static gboolean         state_event_watcher                     (GSignalInvocationHint  *hint,
                                                                 guint                  n_param_values,
                                                                 const GValue           *param_values,
                                                                 gpointer               data);
static void             window_added                             (AtkObject             *atk_obj,
                                                                  guint                 index,
                                                                  AtkObject             *child);
static void             window_removed                           (AtkObject             *atk_obj,
                                                                  guint                 index,
                                                                  AtkObject             *child);
static gboolean        window_focus                              (GtkWidget             *widget,
                                                                  GdkEventFocus         *event);
static gboolean         configure_event_watcher                 (GSignalInvocationHint  *hint,
                                                                 guint                  n_param_values,
                                                                 const GValue           *param_values,
                                                                 gpointer               data);
                                                                  

static AtkObject* root = NULL;
static GHashTable *listener_list = NULL;
static gint listener_idx = 1;
static GSList *key_listener_list = NULL;
static guint key_snooper_id = 0;

typedef struct _GailUtilListenerInfo GailUtilListenerInfo;
typedef struct _GailKeyEventInfo GailKeyEventInfo;

struct _GailUtilListenerInfo
{
   gint key;
   guint signal_id;
   gulong hook_id;
};

struct _GailKeyEventInfo
{
  AtkKeyEventStruct *key_event;
  gpointer func_data;
};

G_DEFINE_TYPE (GailUtil, gail_util, ATK_TYPE_UTIL)

static void	 
gail_util_class_init (GailUtilClass *klass)
{
  AtkUtilClass *atk_class;
  gpointer data;

  data = g_type_class_peek (ATK_TYPE_UTIL);
  atk_class = ATK_UTIL_CLASS (data);

  atk_class->add_global_event_listener =
    gail_util_add_global_event_listener;
  atk_class->remove_global_event_listener =
    gail_util_remove_global_event_listener;
  atk_class->add_key_event_listener =
    gail_util_add_key_event_listener;
  atk_class->remove_key_event_listener =
    gail_util_remove_key_event_listener;
  atk_class->get_root = gail_util_get_root;
  atk_class->get_toolkit_name = gail_util_get_toolkit_name;
  atk_class->get_toolkit_version = gail_util_get_toolkit_version;

  listener_list = g_hash_table_new_full(g_int_hash, g_int_equal, NULL, 
     _listener_info_destroy);
}

static void
gail_util_init (GailUtil *utils)
{
}

static guint
gail_util_add_global_event_listener (GSignalEmissionHook listener,
				     const gchar *event_type)
{
  guint rc = 0;
  gchar **split_string;

  split_string = g_strsplit (event_type, ":", 3);

  if (split_string)
    {
      if (!strcmp ("window", split_string[0]))
        {
          static gboolean initialized = FALSE;

          if (!initialized)
            {
              do_window_event_initialization ();
              initialized = TRUE;
            }
          rc = add_listener (listener, "GailWindow", split_string[1], event_type);
        }
      else
        {
          rc = add_listener (listener, split_string[1], split_string[2], event_type);
        }

      g_strfreev (split_string);
    }

  return rc;
}

static void
gail_util_remove_global_event_listener (guint remove_listener)
{
  if (remove_listener > 0)
  {
    GailUtilListenerInfo *listener_info;
    gint tmp_idx = remove_listener;

    listener_info = (GailUtilListenerInfo *)
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


static
AtkKeyEventStruct *
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

static gint
gail_key_snooper (GtkWidget *the_widget, GdkEventKey *event, gpointer data)
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

  if (key_snooper_id == 0)
    key_snooper_id = gtk_key_snooper_install (gail_key_snooper, NULL);

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

  if (key_listener_list == NULL)
    {
      gtk_key_snooper_remove (key_snooper_id);
      key_snooper_id = 0;
    }
}

static AtkObject*
gail_util_get_root (void)
{
  if (!root)
    {
      root = g_object_new (GAIL_TYPE_TOPLEVEL, NULL);
      atk_object_initialize (root, NULL);
    }

  return root;
}

static const gchar *
gail_util_get_toolkit_name (void)
{
  return "GAIL";
}

static const gchar *
gail_util_get_toolkit_version (void)
{
 /*
  * Version is passed in as a -D flag when this file is
  * compiled.
  */
  return GTK_VERSION;
}

static void
_listener_info_destroy (gpointer data)
{
   g_free(data);
}

static guint
add_listener (GSignalEmissionHook listener,
              const gchar         *object_type,
              const gchar         *signal,
              const gchar         *hook_data)
{
  GType type;
  guint signal_id;
  gint  rc = 0;

  type = g_type_from_name (object_type);
  if (type)
    {
      signal_id  = g_signal_lookup (signal, type);
      if (signal_id > 0)
        {
          GailUtilListenerInfo *listener_info;

          rc = listener_idx;

          listener_info = g_malloc(sizeof(GailUtilListenerInfo));
          listener_info->key = listener_idx;
          listener_info->hook_id =
                          g_signal_add_emission_hook (signal_id, 0, listener,
			        		      g_strdup (hook_data),
			        		      (GDestroyNotify) g_free);
          listener_info->signal_id = signal_id;

	  g_hash_table_insert(listener_list, &(listener_info->key), listener_info);
          listener_idx++;
        }
      else
        {
          g_warning("Invalid signal type %s\n", signal);
        }
    }
  else
    {
      g_warning("Invalid object type %s\n", object_type);
    }
  return rc;
}

static void
do_window_event_initialization (void)
{
  AtkObject *root;

  /*
   * Ensure that GailWindowClass exists.
   */
  g_type_class_ref (GAIL_TYPE_WINDOW);
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

static gboolean
state_event_watcher (GSignalInvocationHint  *hint,
                     guint                  n_param_values,
                     const GValue           *param_values,
                     gpointer               data)
{
  GObject *object;
  GtkWidget *widget;
  AtkObject *atk_obj;
  AtkObject *parent;
  GdkEventWindowState *event;
  gchar *signal_name;
  guint signal_id;

  object = g_value_get_object (param_values + 0);
  /*
   * The object can be a GtkMenu when it is popped up; we ignore this
   */
  if (!GTK_IS_WINDOW (object))
    return FALSE;

  event = g_value_get_boxed (param_values + 1);
  gail_return_val_if_fail (event->type == GDK_WINDOW_STATE, FALSE);
  widget = GTK_WIDGET (object);

  if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
    {
      signal_name = "maximize";
    }
  else if (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED)
    {
      signal_name = "minimize";
    }
  else if (event->new_window_state == 0)
    {
      signal_name = "restore";
    }
  else
    return TRUE;
  
  atk_obj = gtk_widget_get_accessible (widget);

  if (GAIL_IS_WINDOW (atk_obj))
    {
      parent = atk_object_get_parent (atk_obj);
      if (parent == atk_get_root ())
	{
	  signal_id = g_signal_lookup (signal_name, GAIL_TYPE_WINDOW); 
	  g_signal_emit (atk_obj, signal_id, 0);
	}
      
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
window_added (AtkObject *atk_obj,
              guint     index,
              AtkObject *child)
{
  GtkWidget *widget;

  if (!GAIL_IS_WINDOW (child)) return;

  widget = GTK_ACCESSIBLE (child)->widget;
  gail_return_if_fail (widget);

  g_signal_connect (widget, "focus-in-event",  
                    (GCallback) window_focus, NULL);
  g_signal_connect (widget, "focus-out-event",  
                    (GCallback) window_focus, NULL);
  g_signal_emit (child, g_signal_lookup ("create", GAIL_TYPE_WINDOW), 0); 
}


static void
window_removed (AtkObject *atk_obj,
                 guint     index,
                 AtkObject *child)
{
  GtkWidget *widget;
  GtkWindow *window;

  if (!GAIL_IS_WINDOW (child)) return;

  widget = GTK_ACCESSIBLE (child)->widget;
  gail_return_if_fail (widget);

  window = GTK_WINDOW (widget);
  /*
   * Deactivate window if it is still focused and we are removing it. This
   * can happen when a dialog displayed by gok is removed.
   */
  if (window->is_active &&
      window->has_toplevel_focus)
    {
      gchar *signal_name;
      AtkObject *atk_obj;

      atk_obj = gtk_widget_get_accessible (widget);
      signal_name =  "deactivate";
      g_signal_emit (atk_obj, g_signal_lookup (signal_name, GAIL_TYPE_WINDOW), 0); 
    }

  g_signal_handlers_disconnect_by_func (widget, (gpointer) window_focus, NULL);
  g_signal_emit (child, g_signal_lookup ("destroy", GAIL_TYPE_WINDOW), 0); 
}

static gboolean
window_focus (GtkWidget     *widget,
              GdkEventFocus *event)
{
  gchar *signal_name;
  AtkObject *atk_obj;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  atk_obj = gtk_widget_get_accessible (widget);
  signal_name =  (event->in) ? "activate" : "deactivate";
  g_signal_emit (atk_obj, g_signal_lookup (signal_name, GAIL_TYPE_WINDOW), 0); 

  return FALSE;
}

static gboolean 
configure_event_watcher (GSignalInvocationHint  *hint,
                         guint                  n_param_values,
                         const GValue           *param_values,
                         gpointer               data)
{
  GObject *object;
  GtkWidget *widget;
  AtkObject *atk_obj;
  AtkObject *parent;
  GdkEvent *event;
  gchar *signal_name;
  guint signal_id;

  object = g_value_get_object (param_values + 0);
  if (!GTK_IS_WINDOW (object))
    /*
     * GtkDrawingArea can send a GDK_CONFIGURE event but we ignore here
     */
    return FALSE;

  event = g_value_get_boxed (param_values + 1);
  if (event->type != GDK_CONFIGURE)
    return FALSE;
  if (GTK_WINDOW (object)->configure_request_count)
    /*
     * There is another ConfigureRequest pending so we ignore this one.
     */
    return TRUE;
  widget = GTK_WIDGET (object);
  if (widget->allocation.x == ((GdkEventConfigure *)event)->x &&
      widget->allocation.y == ((GdkEventConfigure *)event)->y &&
      widget->allocation.width == ((GdkEventConfigure *)event)->width &&
      widget->allocation.height == ((GdkEventConfigure *)event)->height)
    return TRUE;

  if (widget->allocation.width != ((GdkEventConfigure *)event)->width ||
      widget->allocation.height != ((GdkEventConfigure *)event)->height)
    {
      signal_name = "resize";
    }
  else
    {
      signal_name = "move";
    }

  atk_obj = gtk_widget_get_accessible (widget);
  if (GAIL_IS_WINDOW (atk_obj))
    {
      parent = atk_object_get_parent (atk_obj);
      if (parent == atk_get_root ())
	{
	  signal_id = g_signal_lookup (signal_name, GAIL_TYPE_WINDOW); 
	  g_signal_emit (atk_obj, signal_id, 0);
	}
      
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

G_DEFINE_TYPE (GailMisc, gail_misc, ATK_TYPE_MISC)

static void	 
gail_misc_class_init (GailMiscClass *klass)
{
  AtkMiscClass *miscclass = ATK_MISC_CLASS (klass);
  miscclass->threads_enter =
    gail_misc_threads_enter;
  miscclass->threads_leave =
    gail_misc_threads_leave;
  atk_misc_instance = g_object_new (GAIL_TYPE_MISC, NULL);
}

static void
gail_misc_init (GailMisc *misc)
{
}

static void gail_misc_threads_enter (AtkMisc *misc)
{
  GDK_THREADS_ENTER ();
}

static void gail_misc_threads_leave (AtkMisc *misc)
{
  GDK_THREADS_LEAVE ();
}
