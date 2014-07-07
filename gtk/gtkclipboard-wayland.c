/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2004 Nokia Corporation
 * Copyright (C) 2006-2008 Imendio AB
 * Copyright (C) 2011-2012 Intel Corporation
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
 *
 */

#include "config.h"

#include "gtkclipboard-waylandprivate.h"

#ifdef GDK_WINDOWING_WAYLAND

#include <string.h>

#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"
#include "gtkselectionprivate.h"

static void gtk_clipboard_wayland_owner_change     (GtkClipboard             *clipboard,
                                                    GdkEventOwnerChange      *event);
static gboolean gtk_clipboard_wayland_set_contents (GtkClipboard             *clipboard,
                                                    const GtkTargetEntry     *targets,
                                                    guint                     n_targets,
                                                    GtkClipboardGetFunc       get_func,
                                                    GtkClipboardClearFunc     clear_func,
                                                    gpointer                  user_data,
                                                    gboolean                  have_owner);
static void gtk_clipboard_wayland_clear            (GtkClipboard             *clipboard);
static void gtk_clipboard_wayland_request_contents (GtkClipboard             *clipboard,
                                                    GdkAtom                   target,
                                                    GtkClipboardReceivedFunc  callback,
                                                    gpointer                  user_data);
static void gtk_clipboard_wayland_set_can_store    (GtkClipboard             *clipboard,
                                                    const GtkTargetEntry     *targets,
                                                    gint                      n_targets);
static void gtk_clipboard_wayland_store            (GtkClipboard             *clipboard);

G_DEFINE_TYPE (GtkClipboardWayland, gtk_clipboard_wayland, GTK_TYPE_CLIPBOARD);

static void
gtk_clipboard_wayland_class_init (GtkClipboardWaylandClass *klass)
{
  GtkClipboardClass *clipboard_class = GTK_CLIPBOARD_CLASS (klass);

  clipboard_class->set_contents = gtk_clipboard_wayland_set_contents;
  clipboard_class->clear = gtk_clipboard_wayland_clear;
  clipboard_class->request_contents = gtk_clipboard_wayland_request_contents;
  clipboard_class->set_can_store = gtk_clipboard_wayland_set_can_store;
  clipboard_class->store = gtk_clipboard_wayland_store;
  clipboard_class->owner_change = gtk_clipboard_wayland_owner_change;
}

static void
gtk_clipboard_wayland_init (GtkClipboardWayland *clipboard)
{
}

struct _SetContentClosure {
    GtkClipboard *clipboard;
    GtkClipboardGetFunc get_func;
    GtkClipboardClearFunc clear_func;
    guint info;
    gboolean have_owner;
    gpointer userdata;
    GtkTargetPair *targets;
    gint n_targets;
};

static gchar *
_offer_cb (GdkDevice   *device,
           const gchar *mime_type,
           gssize      *len,
           gpointer     userdata)
{
  SetContentClosure *closure = (SetContentClosure *)userdata;
  GtkSelectionData selection_data = { 0, };
  guint info = 0;
  gint i;

  selection_data.target = gdk_atom_intern (mime_type, FALSE);

  for (i = 0; i < closure->n_targets; i++)
    {
      if (closure->targets[i].target == selection_data.target)
        {
          info = closure->targets[i].info;
          break;
        }
    }

  closure->get_func (closure->clipboard,
                     &selection_data,
                     info,
                     closure->userdata);

  *len = gtk_selection_data_get_length (&selection_data);

  /* The caller of this callback will free this data - the GtkClipboardGetFunc
   * uses gtk_selection_data_set which copies*/
  return (gchar *)selection_data.data;
}

static void
clipboard_owner_destroyed (gpointer data,
			   GObject *owner)
{
  GtkClipboardWayland *clipboard = GTK_CLIPBOARD_WAYLAND (data);
  GtkClipboard *gtkclipboard = GTK_CLIPBOARD (data);
  SetContentClosure *last_closure = clipboard->last_closure;

  last_closure->userdata = NULL;
  last_closure->get_func = NULL;
  last_closure->clear_func = NULL;
  last_closure->have_owner = FALSE;

  gtk_clipboard_clear (gtkclipboard);
}

static gboolean
gtk_clipboard_wayland_set_contents (GtkClipboard         *gtkclipboard,
                                    const GtkTargetEntry *targets,
                                    guint                 n_targets,
                                    GtkClipboardGetFunc   get_func,
                                    GtkClipboardClearFunc clear_func,
                                    gpointer              user_data,
                                    gboolean              have_owner)
{
  GtkClipboardWayland *clipboard = GTK_CLIPBOARD_WAYLAND (gtkclipboard);
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  gint i, j;
  gchar **mimetypes;
  SetContentClosure *closure, *last_closure;

  if (gtkclipboard->selection != GDK_SELECTION_CLIPBOARD)
    return FALSE;

  last_closure = clipboard->last_closure;
  if (!last_closure ||
      (!last_closure->have_owner && have_owner) ||
      (last_closure->userdata != user_data)) {
    gtk_clipboard_clear (gtkclipboard);

    closure = g_new0 (SetContentClosure, 1);
    closure->clipboard = gtkclipboard;
    closure->userdata = user_data;
    closure->have_owner = have_owner;

    if (have_owner)
      g_object_weak_ref (G_OBJECT (user_data), clipboard_owner_destroyed, clipboard);
  } else {
    closure = last_closure;
    g_free (closure->targets);
  }

  closure->get_func = get_func;
  closure->clear_func = clear_func;
  closure->targets = g_new0 (GtkTargetPair, n_targets);

  device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
  device = gdk_device_manager_get_client_pointer (device_manager);

  mimetypes = g_new (gchar *, n_targets);

  for (i = 0, j = 0; i < n_targets; i++)
    {
      if (strcmp(targets[i].target, "COMPOUND_TEXT") == 0)
	continue;
      if (strcmp(targets[i].target, "UTF8_STRING") == 0)
	continue;
      if (strcmp(targets[i].target, "TEXT") == 0)
	continue;
      if (strcmp(targets[i].target, "STRING") == 0)
	continue;
      if (strcmp(targets[i].target, "GTK_TEXT_BUFFER_CONTENTS") == 0)
	continue;

      mimetypes[j] = targets[i].target;
      closure->targets[j].target = gdk_atom_intern (targets[i].target, FALSE);
      closure->targets[j].flags = targets[i].flags;
      closure->targets[j].info = targets[i].info;
      j++;
    }

  closure->n_targets = j;

  gdk_wayland_device_offer_selection_content (device,
                                              (const gchar **)mimetypes,
                                              j,
                                              _offer_cb,
                                              closure);
  clipboard->last_closure = closure;

  g_free (mimetypes);
  return TRUE;
}

static void
gtk_clipboard_wayland_clear (GtkClipboard *gtkclipboard)
{
  GtkClipboardWayland *clipboard = GTK_CLIPBOARD_WAYLAND (gtkclipboard);
  GdkDeviceManager *device_manager;
  GdkDevice *device;

  if (!clipboard->last_closure)
    return;

  device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
  device = gdk_device_manager_get_client_pointer (device_manager);

  gdk_wayland_device_clear_selection_content (device);

  if (clipboard->last_closure->clear_func)
    {
      clipboard->last_closure->clear_func (gtkclipboard,
                                           clipboard->last_closure->userdata);
    }

  if (clipboard->last_closure->have_owner)
    g_object_weak_unref (G_OBJECT (clipboard->last_closure->userdata),
                         clipboard_owner_destroyed, clipboard);
  g_free (clipboard->last_closure->targets);
  g_free (clipboard->last_closure);

  clipboard->last_closure = NULL;
}

typedef struct {
    GtkClipboard *clipboard;
    GCallback cb;
    gpointer userdata;
    GdkAtom target;
} ClipboardRequestClosure;

static void
_request_generic_cb (GdkDevice   *device,
                     const gchar *data,
                     gsize        len,
                     gpointer     userdata)
{
  ClipboardRequestClosure *closure = (ClipboardRequestClosure *)userdata;
  GtkClipboardReceivedFunc cb = (GtkClipboardReceivedFunc)closure->cb;
  GtkSelectionData selection_data;

  selection_data.selection = GDK_SELECTION_CLIPBOARD;
  selection_data.target = closure->target;
  selection_data.type = closure->target;
  selection_data.length = len;
  selection_data.data = (guchar *)data;

  cb (closure->clipboard, &selection_data, closure->userdata);

  g_free (closure);
}

static void
gtk_clipboard_wayland_request_contents (GtkClipboard            *gtkclipboard,
                                        GdkAtom                  target,
                                        GtkClipboardReceivedFunc callback,
                                        gpointer                 user_data)
{
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  ClipboardRequestClosure *closure;
  gchar *mime_type;

  device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
  device = gdk_device_manager_get_client_pointer (device_manager);

  if (target == gdk_atom_intern_static_string ("TARGETS"))
    {
      GtkSelectionData selection_data;
      int n_atoms;
      GdkAtom *atoms;

      selection_data.selection = GDK_NONE;
      selection_data.format = 32;
      selection_data.type = GDK_SELECTION_TYPE_ATOM;

      n_atoms = gdk_wayland_device_get_selection_type_atoms (device, &atoms);
      selection_data.length = n_atoms;
      selection_data.data = (guchar *) atoms;

      callback (gtkclipboard, &selection_data, user_data);
      return;
    }

  /* When GTK+ requests text, it tries UTF8_STRING first and then
   * falls back to COMPOUND_TEXT and then STRING.  We rewrite
   * UTF8_STRING to text/plain;charset=utf-8, and if that doesn't
   * work, just fail the fallback targets.  Maybe we could do this in
   * a generic way that just compares the target against the targets
   * advertised by the data offer. */
  if (target == gdk_atom_intern_static_string ("UTF8_STRING"))
    target = gdk_atom_intern_static_string ("text/plain;charset=utf-8");
  else if (target == gdk_atom_intern_static_string ("COMPOUND_TEXT") ||
	   target == GDK_TARGET_STRING)
    {
      GtkSelectionData selection_data;

      selection_data.selection = GDK_NONE;
      selection_data.target = GDK_NONE;
      selection_data.type = GDK_NONE;
      selection_data.length = 0;
      selection_data.data = NULL;

      callback (gtkclipboard, &selection_data, user_data);
      return;
    }

  closure = g_new0 (ClipboardRequestClosure, 1);
  closure->clipboard = gtkclipboard;
  closure->cb = (GCallback)callback;
  closure->userdata = user_data;
  closure->target = target;

  /* TODO: Do we need to check that target is valid ? */
  mime_type = gdk_atom_name (target);

  gdk_wayland_device_request_selection_content (device,
                                                mime_type,
                                                _request_generic_cb,
                                                closure);

  g_free (mime_type);
}

static void
gtk_clipboard_wayland_owner_change (GtkClipboard        *clipboard,
                                    GdkEventOwnerChange *event)
{

}

static void
gtk_clipboard_wayland_set_can_store (GtkClipboard         *clipboard,
                                     const GtkTargetEntry *targets,
                                     gint                  n_targets)
{
  /* FIXME: Implement */
}

static void
gtk_clipboard_wayland_store (GtkClipboard *clipboard)
{
  /* FIXME: Implement */
}

#endif /* GDK_WINDOWING_WAYLAND */
