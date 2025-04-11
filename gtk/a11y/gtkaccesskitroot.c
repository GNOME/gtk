/* gtkaccesskitroot.c: AccessKit root object
 *
 * Copyright 2024  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkaccesskitrootprivate.h"
#include "gtkaccesskitcontextprivate.h"
#include "gtknative.h"
#include "gtkroot.h"
#include "gtktext.h"

#if defined(GDK_WINDOWING_WIN32)
#include "win32/gdkwin32.h"
#elif defined(GDK_WINDOWING_MACOS)
#include "macos/gdkmacos.h"
#endif

#include <accesskit.h>

struct _GtkAccessKitRoot
{
  GObject parent_instance;

  GtkRoot *root_widget;

  guint32 next_id;
  GHashTable *contexts;
  GArray *update_queue;
  gboolean did_initial_update;
  gboolean requested_initial_tree;

#if defined(GDK_WINDOWING_WIN32)
  accesskit_windows_subclassing_adapter *adapter;
#elif defined(GDK_WINDOWING_MACOS)
  accesskit_macos_subclassing_adapter *adapter;
#elif defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
  accesskit_unix_adapter *adapter;
#endif
};

enum
{
  PROP_ROOT_WIDGET = 1,

  N_PROPS
};

static GParamSpec *obj_props[N_PROPS];

G_DEFINE_TYPE (GtkAccessKitRoot, gtk_accesskit_root, G_TYPE_OBJECT)

static void
gtk_accesskit_root_finalize (GObject *gobject)
{
  GtkAccessKitRoot *self = GTK_ACCESSKIT_ROOT (gobject);

  g_clear_pointer (&self->contexts, g_hash_table_destroy);
  g_clear_pointer (&self->update_queue, g_array_unref);

#if defined(GDK_WINDOWING_WIN32)
  g_clear_pointer (&self->adapter, accesskit_windows_subclassing_adapter_free);
#elif defined(GDK_WINDOWING_MACOS)
  g_clear_pointer (&self->adapter, accesskit_macos_subclassing_adapter_free);
#elif defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
  g_clear_pointer (&self->adapter, accesskit_unix_adapter_free);
#endif

  G_OBJECT_CLASS (gtk_accesskit_root_parent_class)->finalize (gobject);
}

static void
gtk_accesskit_root_dispose (GObject *gobject)
{
  G_OBJECT_CLASS (gtk_accesskit_root_parent_class)->dispose (gobject);
}

static void
gtk_accesskit_root_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkAccessKitRoot *self = GTK_ACCESSKIT_ROOT (gobject);

  switch (prop_id)
    {
    case PROP_ROOT_WIDGET:
      self->root_widget = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_accesskit_root_get_property (GObject    *gobject,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkAccessKitRoot *self = GTK_ACCESSKIT_ROOT (gobject);

  switch (prop_id)
    {
    case PROP_ROOT_WIDGET:
      g_value_set_object (value, self->root_widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static GtkAccessible *
get_focus (GtkAccessKitRoot *self)
{
  GtkWidget *widget = gtk_root_get_focus (self->root_widget);
  GtkAccessible *accessible;

  if (!widget || !gtk_widget_get_mapped (widget))
    return NULL;

  accessible = g_object_ref (GTK_ACCESSIBLE (widget));

  if (GTK_IS_TEXT (accessible))
    {
      GtkAccessible *parent = gtk_accessible_get_accessible_parent (accessible);
      g_object_unref (accessible);
      accessible = parent;
    }

  while (accessible &&
         !gtk_accessible_get_platform_state (accessible, GTK_ACCESSIBLE_PLATFORM_STATE_FOCUSED))
    {
      GtkAccessible *parent = gtk_accessible_get_accessible_parent (accessible);
      g_object_unref (accessible);
      accessible = parent;
    }

  return accessible;
}

static accesskit_tree_update *
new_tree_update (GtkAccessKitRoot *self)
{
  GtkAccessible *focus = get_focus (self);
  GtkATContext *focus_ctx;
  guint32 focus_id;

  if (!focus)
    focus = g_object_ref (GTK_ACCESSIBLE (self->root_widget));

  focus_ctx = gtk_accessible_get_at_context (focus);
  gtk_at_context_realize (focus_ctx);
  focus_id = gtk_accesskit_context_get_id (GTK_ACCESSKIT_CONTEXT (focus_ctx));
  g_object_unref (focus_ctx);
  g_object_unref (focus);

  return accesskit_tree_update_with_focus (focus_id);
}

static void
add_subtree_to_update (GtkAccessKitRoot      *self,
                       accesskit_tree_update *update,
                       GtkAccessible         *accessible)
{
  GtkATContext *ctx = gtk_accessible_get_at_context (accessible);
  GtkAccessKitContext *accesskit_ctx = GTK_ACCESSKIT_CONTEXT (ctx);
  GtkAccessible *child = gtk_accessible_get_first_accessible_child (accessible);

  while (child)
    {
      GtkAccessible *next = gtk_accessible_get_next_accessible_sibling (child);

      add_subtree_to_update (self, update, child);
      g_object_unref (child);
      child = next;
    }

  gtk_at_context_realize (ctx);
  gtk_accesskit_context_add_to_update (accesskit_ctx, update);

  g_object_unref (ctx);
}

static accesskit_tree_update *
build_full_update (void *data)
{
  GtkAccessKitRoot *self = data;
  accesskit_tree_update *update = new_tree_update (self);
  GtkAccessible *root = GTK_ACCESSIBLE (self->root_widget);
  GtkATContext *root_ctx = gtk_accessible_get_at_context (root);
  guint32 root_id;

  gtk_at_context_realize (root_ctx);
  add_subtree_to_update (self, update, root);
  root_id = gtk_accesskit_context_get_id (GTK_ACCESSKIT_CONTEXT (root_ctx));
  accesskit_tree_update_set_tree (update, accesskit_tree_new (root_id));
  g_object_unref (root_ctx);

  return update;
}

#if defined(GDK_WINDOWING_WIN32) || defined(GDK_WINDOWING_MACOS)
static accesskit_tree_update *
request_initial_tree_main_thread (void *data)
{
  GtkAccessKitRoot *self = data;
  accesskit_tree_update *update = build_full_update (self);
  self->requested_initial_tree = TRUE;
  self->did_initial_update = TRUE;
  return update;
}
#endif

static void
update_if_active (GtkAccessKitRoot *self, accesskit_tree_update_factory factory)
{
#if defined(GDK_WINDOWING_WIN32)
  accesskit_windows_queued_events *events =
    accesskit_windows_subclassing_adapter_update_if_active (self->adapter,
                                                            factory, self);
  if (events)
    accesskit_windows_queued_events_raise (events);
#elif defined(GDK_WINDOWING_MACOS)
  accesskit_macos_queued_events *events =
    accesskit_macos_subclassing_adapter_update_if_active (self->adapter,
                                                          factory, self);
  if (events)
    accesskit_macos_queued_events_raise (events);
#elif defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
  accesskit_unix_adapter_update_if_active (self->adapter, factory, self);
#endif
}

static void
queue_tree_update (GtkAccessKitRoot *self)
{
  if (!gtk_widget_get_mapped (GTK_WIDGET (self->root_widget)))
    return;

  GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (self->root_widget));
  gdk_surface_queue_render (surface);
}

#if defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
static accesskit_tree_update *
request_initial_tree_other_thread (void *data)
{
  GtkAccessKitRoot *self = data;

  queue_tree_update (self);
  self->requested_initial_tree = TRUE;

  return NULL;
}
#endif

typedef struct _DoActionData {
  accesskit_action_request *request;
  GtkAccessKitRoot *root;
} DoActionData;

static gboolean
do_action_main_thread (gpointer data_generic)
{
  DoActionData *data = data_generic;

  if (data->request->target == (data->request->target & 0xffffffff))
    {
      guint id = data->request->target & 0xffffffff;
      GtkAccessKitContext *accesskit_ctx =
        g_hash_table_lookup (data->root->contexts, GUINT_TO_POINTER (id));

      if (accesskit_ctx)
        gtk_accesskit_context_do_action (accesskit_ctx, data->request);
    }

  g_object_unref (data->root);
  accesskit_action_request_free (data->request);
  g_free (data);

  return G_SOURCE_REMOVE;
}

static void
do_action (accesskit_action_request *request, void *data)
{
  GtkAccessKitRoot *root = data;
  DoActionData *main_thread_data = g_new0 (DoActionData, 1);

  main_thread_data->request = request;
  main_thread_data->root = g_object_ref (root);
  g_idle_add (do_action_main_thread, main_thread_data);
}

#if defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
static void
add_context_to_unrealize_queue (gpointer key,
                                gpointer value,
                                gpointer user_data)
{
  GtkATContext *ctx = GTK_AT_CONTEXT (value);
  GPtrArray *contexts_to_unrealize = user_data;

  if (!GTK_IS_ROOT (gtk_at_context_get_accessible (ctx)))
    g_ptr_array_add (contexts_to_unrealize, ctx);
}

static gboolean
deactivate_accessibility_main_thread (gpointer data)
{
  GtkAccessKitRoot *self = data;
  GPtrArray *contexts_to_unrealize = g_ptr_array_new ();
  guint i;

  g_clear_pointer (&self->update_queue, g_array_unref);
  self->did_initial_update = FALSE;
  self->requested_initial_tree = FALSE;

  g_hash_table_foreach (self->contexts, add_context_to_unrealize_queue,
                        contexts_to_unrealize);
  for (i = 0; i < contexts_to_unrealize->len; i++)
    gtk_at_context_unrealize (contexts_to_unrealize->pdata[i]);

  g_ptr_array_unref (contexts_to_unrealize);
  g_object_unref (self);
  return G_SOURCE_REMOVE;
}

static void
deactivate_accessibility (void *data)
{
  GtkAccessKitRoot *self = data;
  g_idle_add (deactivate_accessibility_main_thread, g_object_ref (self));
}
#endif

static void
gtk_accesskit_root_constructed (GObject *gobject)
{
  GtkAccessKitRoot *self = GTK_ACCESSKIT_ROOT (gobject);
#if defined(GDK_WINDOWING_WIN32) || defined(GDK_WINDOWING_MACOS)
  GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (self->root_widget));
#endif
#if defined(GDK_WINDOWING_MACOS)
  gpointer window = gdk_macos_surface_get_native_window (GDK_MACOS_SURFACE (surface));
#endif

  g_assert (gtk_native_get_surface (GTK_NATIVE (self->root_widget)) != NULL);

  self->contexts = g_hash_table_new (NULL, NULL);

#if defined(GDK_WINDOWING_WIN32)
  self->adapter =
    accesskit_windows_subclassing_adapter_new (GDK_SURFACE_HWND (surface),
                                               request_initial_tree_main_thread,
                                               self, do_action, self);
#elif defined(GDK_WINDOWING_MACOS)
  self->adapter =
    accesskit_macos_subclassing_adapter_for_window (window,
                                                    request_initial_tree_main_thread,
                                                    self, do_action, self);
#elif defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
  self->adapter =
    accesskit_unix_adapter_new (request_initial_tree_other_thread, self,
                                do_action, self,
                                deactivate_accessibility, self);
#endif

  G_OBJECT_CLASS (gtk_accesskit_root_parent_class)->constructed (gobject);
}

static void
gtk_accesskit_root_class_init (GtkAccessKitRootClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gtk_accesskit_root_constructed;
  gobject_class->set_property = gtk_accesskit_root_set_property;
  gobject_class->get_property = gtk_accesskit_root_get_property;
  gobject_class->dispose = gtk_accesskit_root_dispose;
  gobject_class->finalize = gtk_accesskit_root_finalize;

  obj_props[PROP_ROOT_WIDGET] =
    g_param_spec_object ("root-widget", NULL, NULL,
                         GTK_TYPE_ROOT,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

static void
gtk_accesskit_root_init (GtkAccessKitRoot *self)
{
}

GtkAccessKitRoot *
gtk_accesskit_root_new (GtkRoot *root_widget)
{
  return g_object_new (GTK_TYPE_ACCESSKIT_ROOT,
                       "root-widget", root_widget,
                       NULL);
}

static void
remove_from_update_queue (GtkAccessKitRoot *self, guint id)
{
  guint i;

  if (!self->update_queue)
    return;

  for (i = 0; i < self->update_queue->len; i++)
    {
      if (g_array_index (self->update_queue, guint, i) == id)
        {
          g_array_remove_index (self->update_queue, i);
          return;
        }
    }
}

static void
add_to_update_queue (GtkAccessKitRoot *self, guint id, gboolean force_to_end)
{
  guint i;

  if (!self->update_queue)
    self->update_queue = g_array_new (FALSE, FALSE, sizeof (guint));

  if (force_to_end)
    remove_from_update_queue (self, id);
  else
    {
      for (i = 0; i < self->update_queue->len; i++)
        {
          if (g_array_index (self->update_queue, guint, i) == id)
            return;
        }
    }

  g_array_append_val (self->update_queue, id);
}

guint32
gtk_accesskit_root_new_id (GtkAccessKitRoot *self)
{
  return ++self->next_id;
}

guint32
gtk_accesskit_root_add_context (GtkAccessKitRoot    *self,
                                GtkAccessKitContext *context)
{
  guint32 id = gtk_accesskit_root_new_id (self);
  g_hash_table_insert (self->contexts, GUINT_TO_POINTER (id), context);

  if (self->did_initial_update)
    add_to_update_queue (self, id, FALSE);

  return id;
}

void
gtk_accesskit_root_remove_context (GtkAccessKitRoot *self, guint32 id)
{
  g_hash_table_remove (self->contexts, GUINT_TO_POINTER (id));
  remove_from_update_queue (self, id);
}

static void
add_unrealized_descendants_to_update (GtkAccessKitRoot      *self,
                                      accesskit_tree_update *update,
                                      GtkAccessKitContext   *accesskit_ctx)
{
  GtkATContext *ctx = GTK_AT_CONTEXT (accesskit_ctx);
  GtkAccessible *accessible = gtk_at_context_get_accessible (ctx);
  GtkAccessible *child = gtk_accessible_get_first_accessible_child (accessible);

  while (child)
    {
      GtkAccessible *next = gtk_accessible_get_next_accessible_sibling (child);
      GtkATContext *child_ctx = gtk_accessible_get_at_context (child);

      if (!gtk_at_context_is_realized (child_ctx))
        {
          GtkAccessKitContext *child_accesskit_ctx =
            GTK_ACCESSKIT_CONTEXT (child_ctx);

          gtk_at_context_realize (child_ctx);
          remove_from_update_queue (self,
                                    gtk_accesskit_context_get_id (child_accesskit_ctx));
          add_unrealized_descendants_to_update (self, update,
                                                child_accesskit_ctx);
          gtk_accesskit_context_add_to_update (child_accesskit_ctx, update);
        }

      g_object_unref (child_ctx);
      g_object_unref (child);
      child = next;
    }
}

static accesskit_tree_update *
build_incremental_update (void *data)
{
  GtkAccessKitRoot *self = data;
  accesskit_tree_update *update = new_tree_update (self);

  while (self->update_queue)
    {
      GArray *current_queue = self->update_queue;
      guint i;

      self->update_queue = NULL;

      for (i = 0; i < current_queue->len; i++)
        {
          guint id = g_array_index (current_queue, guint, i);
          GtkAccessKitContext *accesskit_ctx =
            g_hash_table_lookup (self->contexts, GUINT_TO_POINTER (id));
          add_unrealized_descendants_to_update (self, update, accesskit_ctx);
          gtk_accesskit_context_add_to_update (accesskit_ctx, update);
        }

      g_array_unref (current_queue);
    }

  return update;
}

void
gtk_accesskit_root_queue_update (GtkAccessKitRoot *self,
                                 guint32           id,
                                 gboolean          force_to_end)
{
  if (!self->did_initial_update)
    return;

  add_to_update_queue (self, id, force_to_end);

  queue_tree_update (self);
}

void
gtk_accesskit_root_update_tree (GtkAccessKitRoot *self)
{
  if (self->did_initial_update)
    {
      if (self->update_queue)
        update_if_active (self, build_incremental_update);
    }
  else if (self->requested_initial_tree)
    {
      update_if_active (self, build_full_update);
      self->did_initial_update = TRUE;
    }
}

void
gtk_accesskit_root_update_window_focus_state (GtkAccessKitRoot *self,
                                              gboolean          focused)
{
#if defined(GDK_WINDOWING_MACOS)
  accesskit_macos_queued_events *events =
    accesskit_macos_subclassing_adapter_update_view_focus_state (self->adapter,
                                                                 focused);
  if (events)
    accesskit_macos_queued_events_raise (events);
#elif defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
  accesskit_unix_adapter_update_window_focus_state (self->adapter, focused);
#endif
}
