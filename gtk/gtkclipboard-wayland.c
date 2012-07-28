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
#include <string.h>

#include "gtkclipboard.h"
#include "gtkinvisible.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"
#include "gtktextbuffer.h"
#include "gtkselectionprivate.h"

#include "../gdk/gdk.h"
#include "../gdk/wayland/gdkwayland.h"

enum {
    OWNER_CHANGE,
    LAST_SIGNAL
};

typedef struct _GtkClipboardClass GtkClipboardClass;

typedef struct _SetContentClosure SetContentClosure;
struct _GtkClipboard
{
  GObject parent_instance;

  GObject *owner;
  GdkDisplay *display;

  GdkAtom selection;

  SetContentClosure *last_closure;
};

struct _GtkClipboardClass
{
  GObjectClass parent_class;

  void (*owner_change) (GtkClipboard        *clipboard,
                        GdkEventOwnerChange *event);
};

static void gtk_clipboard_class_init   (GtkClipboardClass   *class);
static void gtk_clipboard_finalize     (GObject             *object);
static void gtk_clipboard_owner_change (GtkClipboard        *clipboard,
                                        GdkEventOwnerChange *event);

static GObjectClass *parent_class;
static guint         clipboard_signals[LAST_SIGNAL] = { 0 };

GType
gtk_clipboard_get_type (void)
{
  static GType clipboard_type = 0;

  if (!clipboard_type)
    {
      const GTypeInfo clipboard_info =
        {
          sizeof (GtkClipboardClass),
          NULL,           /* base_init */
          NULL,           /* base_finalize */
          (GClassInitFunc) gtk_clipboard_class_init,
          NULL,           /* class_finalize */
          NULL,           /* class_data */
          sizeof (GtkClipboard),
          0,              /* n_preallocs */
          (GInstanceInitFunc) NULL,
        };

      clipboard_type = g_type_register_static (G_TYPE_OBJECT, I_("GtkClipboard"),
                                               &clipboard_info, 0);
    }

  return clipboard_type;
}

static void
gtk_clipboard_class_init (GtkClipboardClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gtk_clipboard_finalize;

  class->owner_change = gtk_clipboard_owner_change;

  clipboard_signals[OWNER_CHANGE] =
    g_signal_new (I_("owner-change"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkClipboardClass, owner_change),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
gtk_clipboard_finalize (GObject *object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clipboard_display_closed (GdkDisplay   *display,
			  gboolean      is_error,
			  GtkClipboard *clipboard)
{
  GSList *clipboards;

  clipboards = g_object_get_data (G_OBJECT (display), "gtk-clipboard-list");
  g_object_run_dispose (G_OBJECT (clipboard));
  clipboards = g_slist_remove (clipboards, clipboard);
  g_object_set_data (G_OBJECT (display), I_("gtk-clipboard-list"), clipboards);
  g_object_unref (clipboard);
}

static GtkClipboard *
clipboard_peek (GdkDisplay *display, 
		GdkAtom     selection,
		gboolean    only_if_exists)
{
  GtkClipboard *clipboard = NULL;
  GSList *clipboards;
  GSList *tmp_list;

  if (selection == GDK_NONE)
    selection = GDK_SELECTION_CLIPBOARD;

  clipboards = g_object_get_data (G_OBJECT (display), "gtk-clipboard-list");

  tmp_list = clipboards;
  while (tmp_list)
    {
      clipboard = tmp_list->data;
      if (clipboard->selection == selection)
        break;

      tmp_list = tmp_list->next;
    }

  if (!tmp_list && !only_if_exists)
    {
      clipboard = g_object_new (GTK_TYPE_CLIPBOARD, NULL);
      clipboard->selection = selection;
      clipboard->display = display;

      clipboards = g_slist_prepend (clipboards, clipboard);
      g_object_set_data (G_OBJECT (display), I_("gtk-clipboard-list"), clipboards);
      g_signal_connect (display, "closed",
                        G_CALLBACK (clipboard_display_closed), clipboard);
      gdk_display_request_selection_notification (display, selection);
    }
  
  return clipboard;
}

GtkClipboard *
gtk_clipboard_get_for_display (GdkDisplay *display,
                               GdkAtom     selection)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (!gdk_display_is_closed (display), NULL);

  return clipboard_peek (display, selection, FALSE);
}

GtkClipboard *
gtk_clipboard_get (GdkAtom selection)
{
  return gtk_clipboard_get_for_display (gdk_display_get_default (), selection);
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
  GtkClipboard *clipboard = (GtkClipboard *) data;
  SetContentClosure *last_closure = clipboard->last_closure;

  last_closure->userdata = NULL;
  last_closure->get_func = NULL;
  last_closure->clear_func = NULL;
  last_closure->have_owner = FALSE;

  gtk_clipboard_clear (clipboard);
}

static gboolean
gtk_clipboard_set_contents (GtkClipboard         *clipboard,
                            const GtkTargetEntry *targets,
                            guint                 n_targets,
                            GtkClipboardGetFunc   get_func,
                            GtkClipboardClearFunc clear_func,
                            gpointer              user_data,
                            gboolean              have_owner)
{
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  gint i;
  gchar **mimetypes;
  SetContentClosure *closure, *last_closure;

  last_closure = clipboard->last_closure;
  if (!last_closure ||
      (!last_closure->have_owner && have_owner) ||
      (last_closure->userdata != user_data)) {
    gtk_clipboard_clear (clipboard);

    closure = g_new0 (SetContentClosure, 1);
    closure->clipboard = clipboard;
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
  closure->n_targets = n_targets;

  device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
  device = gdk_device_manager_get_client_pointer (device_manager);

  mimetypes = g_new (gchar *, n_targets);

  for (i = 0; i < n_targets; i++)
    {
      mimetypes[i] = targets[i].target;
      closure->targets[i].target = gdk_atom_intern (targets[i].target, FALSE);
      closure->targets[i].flags = targets[i].flags;
      closure->targets[i].info = targets[i].info;
    }

  gdk_wayland_device_offer_selection_content (device,
                                              (const gchar **)mimetypes,
                                              n_targets,
                                              _offer_cb,
                                              closure);
  clipboard->last_closure = closure;

  g_free (mimetypes);
  return TRUE;
}

gboolean
gtk_clipboard_set_with_data (GtkClipboard          *clipboard,
                             const GtkTargetEntry  *targets,
                             guint                  n_targets,
                             GtkClipboardGetFunc    get_func,
                             GtkClipboardClearFunc  clear_func,
                             gpointer               user_data)
{
  g_return_val_if_fail (clipboard != NULL, FALSE);
  g_return_val_if_fail (targets != NULL, FALSE);
  g_return_val_if_fail (get_func != NULL, FALSE);

  return gtk_clipboard_set_contents (clipboard, targets, n_targets,
                                     get_func, clear_func, user_data,
                                     FALSE);
}

gboolean
gtk_clipboard_set_with_owner (GtkClipboard          *clipboard,
                              const GtkTargetEntry  *targets,
                              guint                  n_targets,
                              GtkClipboardGetFunc    get_func,
                              GtkClipboardClearFunc  clear_func,
                              GObject               *owner)
{
  g_return_val_if_fail (clipboard != NULL, FALSE);
  g_return_val_if_fail (targets != NULL, FALSE);
  g_return_val_if_fail (get_func != NULL, FALSE);
  g_return_val_if_fail (G_IS_OBJECT (owner), FALSE);

  return gtk_clipboard_set_contents (clipboard, targets, n_targets,
                                     get_func, clear_func, owner,
                                     TRUE);
}

GObject *
gtk_clipboard_get_owner (GtkClipboard *clipboard)
{
  g_return_val_if_fail (clipboard != NULL, NULL);

  if (clipboard->owner)
    return clipboard->owner;
  else
    return NULL;
}

void
gtk_clipboard_clear (GtkClipboard *clipboard)
{
  GdkDeviceManager *device_manager;
  GdkDevice *device;

  if (!clipboard->last_closure)
    return;

  device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
  device = gdk_device_manager_get_client_pointer (device_manager);

  gdk_wayland_device_clear_selection_content (device);

  if (clipboard->last_closure->clear_func)
    {
      clipboard->last_closure->clear_func (clipboard,
                                           clipboard->last_closure->userdata);
    }

  if (clipboard->last_closure->have_owner)
    g_object_weak_unref (G_OBJECT (clipboard->last_closure->userdata),
                         clipboard_owner_destroyed, clipboard);
  g_free (clipboard->last_closure->targets);
  g_free (clipboard->last_closure);

  clipboard->last_closure = NULL;
}

static void 
text_get_func (GtkClipboard     *clipboard,
               GtkSelectionData *selection_data,
               guint             info,
               gpointer          data)
{
  gtk_selection_data_set_text (selection_data, data, -1);
}

static void 
text_clear_func (GtkClipboard *clipboard,
                 gpointer      data)
{
  g_free (data);
}

void
gtk_clipboard_set_text (GtkClipboard *clipboard,
                        const gchar  *text,
                        gint          len)
{
  GtkTargetEntry target = { "text/plain;charset=utf-8", 0, 0 };

  g_return_if_fail (clipboard != NULL);
  g_return_if_fail (text != NULL);

  if (len < 0)
    len = strlen (text);

  gtk_clipboard_set_with_data (clipboard, 
                               &target, 1,
                               text_get_func, text_clear_func,
                               g_strndup (text, len));
  gtk_clipboard_set_can_store (clipboard, NULL, 0);
}


static void
pixbuf_get_func (GtkClipboard     *clipboard,
                 GtkSelectionData *selection_data,
                 guint             info,
                 gpointer          data)
{
  gtk_selection_data_set_pixbuf (selection_data, data);
}

static void 
pixbuf_clear_func (GtkClipboard *clipboard,
                   gpointer      data)
{
  g_object_unref (data);
}

void
gtk_clipboard_set_image (GtkClipboard *clipboard,
                         GdkPixbuf    *pixbuf)
{
  GtkTargetList *list;
  GList *l;
  GtkTargetEntry *targets;
  gint n_targets, i;

  g_return_if_fail (clipboard != NULL);
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_image_targets (list, 0, TRUE);

  n_targets = g_list_length (list->list);
  targets = g_new0 (GtkTargetEntry, n_targets);
  for (l = list->list, i = 0; l; l = l->next, i++)
    {
      GtkTargetPair *pair = (GtkTargetPair *)l->data;
      targets[i].target = gdk_atom_name (pair->target);
    }

  gtk_clipboard_set_with_data (clipboard, 
                               targets, n_targets,
                               pixbuf_get_func, pixbuf_clear_func,
                               g_object_ref (pixbuf));
  gtk_clipboard_set_can_store (clipboard, NULL, 0);

  for (i = 0; i < n_targets; i++)
    g_free (targets[i].target);
  g_free (targets);
  gtk_target_list_unref (list);
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
  selection_data.length = len;
  selection_data.data = (guchar *)data;

  cb (closure->clipboard, &selection_data, closure->userdata);

  g_free (closure);
}

void
gtk_clipboard_request_contents (GtkClipboard            *clipboard,
                                GdkAtom                  target,
                                GtkClipboardReceivedFunc callback,
                                gpointer                 user_data)
{
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  ClipboardRequestClosure *closure;

  device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
  device = gdk_device_manager_get_client_pointer (device_manager);

  closure = g_new0 (ClipboardRequestClosure, 1);
  closure->clipboard = clipboard;
  closure->cb = (GCallback)callback;
  closure->userdata = user_data;
  closure->target = target;

  /* TODO: Do we need to check that target is valid ? */
  gdk_wayland_device_request_selection_content (device,
                                                gdk_atom_name (target),
                                                _request_generic_cb,
                                                closure);
}

static void
_request_text_cb (GdkDevice   *device,
                  const gchar *data,
                  gsize        len,
                  gpointer     userdata)
{
  ClipboardRequestClosure *closure = (ClipboardRequestClosure *)userdata;
  GtkClipboardTextReceivedFunc cb = (GtkClipboardTextReceivedFunc)closure->cb;

  cb (closure->clipboard, data, closure->userdata);

  g_free (closure);
}

void
gtk_clipboard_request_text (GtkClipboard                *clipboard,
                            GtkClipboardTextReceivedFunc callback,
                            gpointer                     user_data)
{
  GdkDeviceManager *device_manager;
  GdkDevice *device;

  ClipboardRequestClosure *closure;

  device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
  device = gdk_device_manager_get_client_pointer (device_manager);

  closure = g_new0 (ClipboardRequestClosure, 1);
  closure->clipboard = clipboard;
  closure->cb = (GCallback)callback;
  closure->userdata = user_data;
  gdk_wayland_device_request_selection_content (device,
                                                "text/plain;charset=utf-8",
                                                _request_text_cb,
                                                closure);
}

void
gtk_clipboard_request_rich_text (GtkClipboard                    *clipboard,
                                 GtkTextBuffer                   *buffer,
                                 GtkClipboardRichTextReceivedFunc callback,
                                 gpointer                         user_data)
{
  /* FIXME: Implement */
}

void
gtk_clipboard_request_image (GtkClipboard                  *clipboard,
                             GtkClipboardImageReceivedFunc  callback,
                             gpointer                       user_data)
{
  /* FIXME: Implement */
}

void
gtk_clipboard_request_uris (GtkClipboard                *clipboard,
                            GtkClipboardURIReceivedFunc  callback,
                            gpointer                     user_data)
{
  /* FIXME: Implement */
}

void
gtk_clipboard_request_targets (GtkClipboard                    *clipboard,
                               GtkClipboardTargetsReceivedFunc  callback,
                               gpointer                         user_data)
{
  GdkAtom *targets;
  gint n_targets;

  gtk_clipboard_wait_for_targets (clipboard, &targets, &n_targets);

  callback (clipboard, targets, n_targets, user_data);

  g_free (targets);
}

typedef struct {
    GMainLoop *loop;
    GtkSelectionData *selection_data;
} WaitClosure;

static void
_wait_for_contents_cb (GtkClipboard     *clipboard,
                       GtkSelectionData *selection_data,
                       gpointer          userdata)
{
  WaitClosure *closure = (WaitClosure *)userdata;

  if (gtk_selection_data_get_length (selection_data) != -1)
    closure->selection_data = gtk_selection_data_copy (selection_data);

  g_main_loop_quit (closure->loop);
}

GtkSelectionData *
gtk_clipboard_wait_for_contents (GtkClipboard *clipboard,
                                 GdkAtom       target)
{
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  WaitClosure *closure;
  GtkSelectionData *selection_data = NULL;

  g_return_val_if_fail (clipboard != NULL, NULL);
  g_return_val_if_fail (target != GDK_NONE, NULL);

  device_manager = gdk_display_get_device_manager (clipboard->display);
  device = gdk_device_manager_get_client_pointer (device_manager);

  if (target == gdk_atom_intern_static_string ("TARGETS")) 
    {
      GdkAtom *atoms;
      gint nr_atoms;

      selection_data = g_slice_new0 (GtkSelectionData);
      selection_data->selection = GDK_SELECTION_CLIPBOARD;
      selection_data->target = target;

      nr_atoms = gdk_wayland_device_get_selection_type_atoms (device, &atoms);
      gtk_selection_data_set (selection_data,
                              GDK_SELECTION_TYPE_ATOM, 32,
                              (guchar *)atoms,
                              32 * nr_atoms);

      g_free (atoms);

      return selection_data;
    }

  closure = g_new0 (WaitClosure, 1);
  closure->selection_data = NULL;
  closure->loop = g_main_loop_new (NULL, TRUE);

  gtk_clipboard_request_contents (clipboard,
                                  target,
                                  _wait_for_contents_cb,
                                  closure);

  if (g_main_loop_is_running (closure->loop))
    {
      gdk_threads_leave ();
      g_main_loop_run (closure->loop);
      gdk_threads_enter ();
    }

  g_main_loop_unref (closure->loop);

  selection_data = closure->selection_data;

  g_free (closure);

  return selection_data;
}

gchar *
gtk_clipboard_wait_for_text (GtkClipboard *clipboard)
{
  GtkSelectionData *data;
  gchar *result;

  data =
    gtk_clipboard_wait_for_contents (clipboard,
                                     gdk_atom_intern_static_string ("text/plain;charset=utf-8"));

  result = (gchar *)gtk_selection_data_get_text (data);

  gtk_selection_data_free (data);

  return result;
}

GdkPixbuf *
gtk_clipboard_wait_for_image (GtkClipboard *clipboard)
{
  const gchar *priority[] = { "image/png",
      "image/tiff",
      "image/jpeg",
      "image/gif",
      "image/bmp" };
  int i;
  GtkSelectionData *data;

  for (i = 0; i < G_N_ELEMENTS (priority); i++) 
    {
      data = gtk_clipboard_wait_for_contents (clipboard, gdk_atom_intern_static_string (priority[i]));

      if (data)
        {
          GdkPixbuf *pixbuf = gtk_selection_data_get_pixbuf (data);

          gtk_selection_data_free (data);

          return pixbuf;
        }
    }

  return NULL;
}

guint8 *
gtk_clipboard_wait_for_rich_text (GtkClipboard  *clipboard,
                                  GtkTextBuffer *buffer,
                                  GdkAtom       *format,
                                  gsize         *length)
{
  /* FIXME: Implement */
  return NULL;
}

gchar **
gtk_clipboard_wait_for_uris (GtkClipboard *clipboard)
{
  GtkSelectionData *data;

  data =
    gtk_clipboard_wait_for_contents (clipboard,
                                     gdk_atom_intern_static_string ("text/uri-list"));
  if (data)
    {
      gchar **uris;

      uris = gtk_selection_data_get_uris (data);
      gtk_selection_data_free (data);

      return uris;
    }  

  return NULL;
}

GdkDisplay *
gtk_clipboard_get_display (GtkClipboard *clipboard)
{
  g_return_val_if_fail (clipboard != NULL, NULL);

  return clipboard->display;
}

gboolean
gtk_clipboard_wait_is_text_available (GtkClipboard *clipboard)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  data =
    gtk_clipboard_wait_for_contents (clipboard,
                                     gdk_atom_intern_static_string ("TARGETS"));
  if (data)
    {
      result = gtk_selection_data_targets_include_text (data);
      gtk_selection_data_free (data);
    }

  return result;
}

gboolean
gtk_clipboard_wait_is_rich_text_available (GtkClipboard  *clipboard,
                                           GtkTextBuffer *buffer)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  g_return_val_if_fail (GTK_IS_CLIPBOARD (clipboard), FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);

  data =
    gtk_clipboard_wait_for_contents (clipboard,
                                     gdk_atom_intern_static_string ("TARGETS"));
  if (data)
    {
      result = gtk_selection_data_targets_include_rich_text (data, buffer);
      gtk_selection_data_free (data);
    }

  return result;
}

gboolean
gtk_clipboard_wait_is_image_available (GtkClipboard *clipboard)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  data =
    gtk_clipboard_wait_for_contents (clipboard, 
                                     gdk_atom_intern_static_string ("TARGETS"));
  if (data)
    {
      result = gtk_selection_data_targets_include_image (data, FALSE);
      gtk_selection_data_free (data);
    }

  return result;
}

gboolean
gtk_clipboard_wait_is_uris_available (GtkClipboard *clipboard)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  data =
    gtk_clipboard_wait_for_contents (clipboard, 
                                     gdk_atom_intern_static_string ("TARGETS"));
  if (data)
    {
      result = gtk_selection_data_targets_include_uri (data);
      gtk_selection_data_free (data);
    }

  return result;
}

gboolean
gtk_clipboard_wait_for_targets (GtkClipboard  *clipboard,
                                GdkAtom      **targets,
                                gint          *n_targets)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  g_return_val_if_fail (clipboard != NULL, FALSE);

  data =
    gtk_clipboard_wait_for_contents (clipboard,
                                     gdk_atom_intern_static_string ("TARGETS"));

  if (data)
    {
      GdkAtom *tmp_targets;
      gint tmp_n_targets;

      result = gtk_selection_data_get_targets (data,
                                               &tmp_targets,
                                               &tmp_n_targets);

      if (n_targets)
        *n_targets = tmp_n_targets;

      if (targets)
        *targets = tmp_targets;
      else
        g_free (tmp_targets);

      gtk_selection_data_free (data);
    }

  return result;
}

static void
gtk_clipboard_owner_change (GtkClipboard        *clipboard,
                            GdkEventOwnerChange *event)
{

}

gboolean
gtk_clipboard_wait_is_target_available (GtkClipboard *clipboard,
                                        GdkAtom       target)
{
  GdkAtom *targets;
  gint i, n_targets;
  gboolean retval = FALSE;

  g_return_val_if_fail (clipboard != NULL, FALSE);

  if (!gtk_clipboard_wait_for_targets (clipboard, &targets, &n_targets))
    return FALSE;

  for (i = 0; i < n_targets; i++)
    {
      if (targets[i] == target)
        {
          retval = TRUE;
          break;
        }
    }

  g_free (targets);

  return retval;
}

void
_gtk_clipboard_handle_event (GdkEventOwnerChange *event)
{
}

void
gtk_clipboard_set_can_store (GtkClipboard         *clipboard,
                             const GtkTargetEntry *targets,
                             gint                  n_targets)
{
  /* FIXME: Implement */
}

void
gtk_clipboard_store (GtkClipboard *clipboard)
{
  /* FIXME: Implement */
}

void
_gtk_clipboard_store_all (void)
{
  /* FIXME: Implement */
}
