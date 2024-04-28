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

#if defined(GDK_WINDOWING_WIN32)
#include "win32/gdkwin32.h"
#elif defined(GDK_WINDOWING_WAYLAND)
#include "wayland/gdkwayland.h"
#endif

#include <accesskit.h>

struct _GtkAccessKitRoot
{
  GObject parent_instance;

  GtkRoot *root_widget;

  guint32 next_id;
  GHashTable *contexts;
  GSList *update_queue;
  gboolean did_initial_update;
  gint update_id;

#if defined(GDK_WINDOWING_WIN32)
  accesskit_windows_subclassing_adapter *adapter;
#elif defined(GDK_WINDOWING_WAYLAND)
  accesskit_newton_adapter *adapter;
/* TODO: other platforms */
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
  g_clear_pointer (&self->update_queue, g_slist_free);
  g_clear_handle_id (&self->update_id, g_source_remove);

#if defined(GDK_WINDOWING_WIN32)
  g_clear_pointer (&self->adapter, accesskit_windows_subclassing_adapter_free);
#elif defined(GDK_WINDOWING_WAYLAND)
  g_clear_pointer (&self->adapter, accesskit_newton_adapter_free);
/* TODO: other platforms */
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

static accesskit_tree_update *
new_tree_update (GtkAccessKitRoot *self)
{
  GtkWidget *focus = gtk_root_get_focus (self->root_widget);
  GtkATContext *focus_ctx;
  guint32 focus_id;

  if (!focus)
    focus = GTK_WIDGET (self->root_widget);

  focus_ctx = gtk_accessible_get_at_context (GTK_ACCESSIBLE (focus));
  gtk_at_context_realize (focus_ctx);
  focus_id = gtk_accesskit_context_get_id (GTK_ACCESSKIT_CONTEXT (focus_ctx));
  g_object_unref (focus_ctx);

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

  g_assert (gtk_at_context_is_realized (ctx));
  gtk_accesskit_context_add_to_update (accesskit_ctx, update);

  while (child)
    {
      GtkAccessible *next = gtk_accessible_get_next_accessible_sibling (child);

      add_subtree_to_update (self, update, child);
      g_object_unref (child);
      child = next;
    }

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

static accesskit_tree_update *
request_initial_tree_main_thread (void *data)
{
  GtkAccessKitRoot *self = data;
  accesskit_tree_update *update = build_full_update (self);
  self->did_initial_update = TRUE;
  return update;
}

static void
update_if_active (GtkAccessKitRoot *self, accesskit_tree_update_factory factory)
{
#if defined(GDK_WINDOWING_WIN32)
  accesskit_windows_queued_events *events =
    accesskit_windows_subclassing_adapter_update_if_active (self->adapter,
                                                            factory, self);
  if (events)
    accesskit_windows_queued_events_raise (events);
#elif defined(GDK_WINDOWING_WAYLAND)
  /* TBD: Newton treats accessibility tree updates as double-buffered state,
     meaning the surface has to be committed after the update is sent.
     Can we more tightly integrate accessibility updates with rendering,
     so everything happens atomically as intended, rather than queuing
     an idle callback for the accessibility update, then committing
     the surface? */
  GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (self->root_widget));
  struct wl_surface *wl_surface =
    gdk_wayland_surface_get_wl_surface (GDK_WAYLAND_SURFACE (surface));

  accesskit_newton_adapter_update_if_active (self->adapter, factory, self);
  wl_surface_commit (wl_surface);
/* TODO: other platforms */
#endif
}

static gboolean
initial_update (gpointer data)
{
  GtkAccessKitRoot *self = data;

  self->update_id = 0;
  update_if_active (self, build_full_update);
  self->did_initial_update = TRUE;

  return G_SOURCE_REMOVE;
}

static accesskit_tree_update *
request_initial_tree_other_thread (void *data)
{
  GtkAccessKitRoot *self = data;

  if (!self->update_id)
    self->update_id = g_idle_add (initial_update, self);

  return NULL;
}

static void
do_action (const accesskit_action_request *request, void *data)
{
  /* TODO */
}

static void
deactivate_accessibility (void *data)
{
  GtkAccessKitRoot *self = data;

  g_clear_pointer (&self->contexts, g_slist_free);
  g_clear_handle_id (&self->update_id, g_source_remove);
  self->did_initial_update = FALSE;
}

static void
gtk_accesskit_root_constructed (GObject *gobject)
{
  GtkAccessKitRoot *self = GTK_ACCESSKIT_ROOT (gobject);
  GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (self->root_widget));

  self->contexts = g_hash_table_new (NULL, NULL);

#if defined(GDK_WINDOWING_WIN32)
  self->adapter =
    accesskit_windows_subclassing_adapter_new (GDK_SURFACE_HWND (surface),
                                               request_initial_tree_main_thread,
                                               self, do_action, self);
#elif defined(GDK_WINDOWING_WAYLAND)
  GdkDisplay *display = gtk_root_get_display (self->root_widget);
  struct wl_display *wl_display =
    gdk_wayland_display_get_wl_display (GDK_WAYLAND_DISPLAY (display));
  struct wl_surface *wl_surface =
    gdk_wayland_surface_get_wl_surface (GDK_WAYLAND_SURFACE (surface));
  self->adapter =
    accesskit_newton_adapter_new (wl_display, wl_surface,
                                  request_initial_tree_other_thread, self,
                                  do_action, self,
                                  deactivate_accessibility, self);
/* TODO: other platforms */
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
add_to_update_queue (GtkAccessKitRoot *self, guint id)
{
  gpointer data = GUINT_TO_POINTER (id);
  GSList *l;

  for (l = self->update_queue; l; l = l->next)
    {
      if (l->data == data)
        return;
    }

  self->update_queue = g_slist_prepend (self->update_queue, data);
}

guint32
gtk_accesskit_root_add_context (GtkAccessKitRoot    *self,
                                GtkAccessKitContext *context)
{
  guint32 id = ++self->next_id;
  g_hash_table_insert (self->contexts, GUINT_TO_POINTER (id), context);

  if (self->did_initial_update)
    add_to_update_queue (self, id);

  return id;
}

void
gtk_accesskit_root_remove_context (GtkAccessKitRoot *self, guint32 id)
{
  g_hash_table_remove (self->contexts, GUINT_TO_POINTER (id));
  self->update_queue = g_slist_remove (self->update_queue, GUINT_TO_POINTER (id));
}

static accesskit_tree_update *
build_incremental_update (void *data)
{
  GtkAccessKitRoot *self = data;
  accesskit_tree_update *update = new_tree_update (self);

  while (self->update_queue)
    {
      GSList *current_queue = self->update_queue;
      GSList *l;

      self->update_queue = NULL;

      for (l = current_queue; l; l = l->next)
        {
          guint32 id = GPOINTER_TO_UINT (l->data);
          GtkAccessKitContext *accesskit_ctx =
            g_hash_table_lookup (self->contexts, l->data);
          gtk_accesskit_context_add_to_update (accesskit_ctx, update);
        }

      g_slist_free (current_queue);
    }

  return update;
}

static gboolean
incremental_update (gpointer data)
{
  GtkAccessKitRoot *self = data;

  self->update_id = 0;
  update_if_active (self, build_incremental_update);

  return G_SOURCE_REMOVE;
}

void
gtk_accesskit_root_queue_update (GtkAccessKitRoot *self, guint32 id)
{
  if (!self->did_initial_update)
    return;

  add_to_update_queue (self, id);

  if (!self->update_id)
    self->update_id = g_idle_add (incremental_update, self);
}
