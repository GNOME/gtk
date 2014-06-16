/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2014 Red Hat, Inc.
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

/* Open issues:
 * - suppport other image formats
 * - implement MULTIPLE
 * - implement INCR
 * - cache remote data and shortcut
 */

#include "config.h"

#include "gdkclipboardprivate.h"
#include "gdkclipboard-x11.h"
#include "gdkprivate-x11.h"

#include <string.h>
#include <X11/Xatom.h>

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#define IDLE_ABORT_TIME 30 /* seconds */

typedef struct _GdkClipboardX11Class GdkClipboardX11Class;

typedef struct _RetrievalInfo RetrievalInfo;

struct _GdkClipboardX11
{
  GdkClipboard parent;

  GdkDisplay *display;
  GdkWindow  *owner;
  Display    *xdisplay;
  Window      xowner;
  Atom        xselection;
  guint32     time;
  gboolean    is_owner;

  Atom                  *targets;
  gint                   n_targets;
  gchar                 *text;
  GdkPixbuf             *pixbuf;
  GdkClipboardProvider   provider;
  gpointer               data;
  GDestroyNotify         destroy;

  GList      *retrievals;
};

struct _GdkClipboardX11Class
{
  GdkClipboardClass parent_class;
};

G_DEFINE_TYPE (GdkClipboardX11, gdk_clipboard_x11, GDK_TYPE_CLIPBOARD)

static Atom targets_atom;
static Atom timestamp_atom;
static Atom multiple_atom;
static Atom incr_atom;
static Atom utf8_string_atom;
static Atom string_atom;
static Atom text_atom;
static Atom ctext_atom;
static Atom text_plain_atom;
static Atom text_plain_utf8_atom;
static Atom text_plain_locale_atom;
static Atom image_png_atom;

static void
init_atoms (Display *display)
{
  const gchar *charset;
  gchar *tmp;

  if (utf8_string_atom != 0)
    return;
  
  targets_atom = XInternAtom (display, "TARGETS", FALSE);
  timestamp_atom = XInternAtom (display, "TIMESTAMP", FALSE);
  multiple_atom = XInternAtom (display, "MULTIPLE", FALSE);
  incr_atom = XInternAtom (display, "INCR", FALSE);
  utf8_string_atom = XInternAtom (display, "UTF8_STRING", FALSE);
  string_atom = XInternAtom (display, "STRING", FALSE);
  text_atom = XInternAtom (display, "TEXT", FALSE);
  ctext_atom = XInternAtom (display, "COMPOUND_TEXT", FALSE);
  text_plain_atom = XInternAtom (display, "text/plain", FALSE);
  text_plain_utf8_atom = XInternAtom (display, "text/plain;charset=utf-8", FALSE);
  g_get_charset (&charset);
  tmp = g_strconcat ("text/plain;charset=", charset, NULL);
  text_plain_locale_atom = XInternAtom (display, tmp, FALSE);
  g_free (tmp);
  image_png_atom = XInternAtom (display, "image/png", FALSE);
}

static guint32
get_timestamp (GdkClipboardX11 *cb)
{
  return gdk_x11_get_server_time (cb->owner);
}

static gboolean
claim_selection (GdkClipboardX11 *cb)
{
  cb->time = get_timestamp (cb);
  XSetSelectionOwner (cb->xdisplay, cb->xselection, cb->xowner, cb->time);
  cb->is_owner = XGetSelectionOwner (cb->xdisplay, cb->xselection) == cb->xowner;

  return cb->is_owner;
}

static void
drop_selection (GdkClipboardX11 *cb)
{
  if (cb->is_owner)
    {
      XSetSelectionOwner (cb->xdisplay, cb->xselection, None, CurrentTime);
      cb->is_owner = FALSE;
    }
}

static void
clear_data (GdkClipboardX11 *cb)
{
  g_free (cb->text);
  cb->text = NULL;
  if (cb->pixbuf)
    {
      g_object_unref (cb->pixbuf);
      cb->pixbuf = NULL;
    }
  if (cb->provider)
    {
      g_free (cb->targets);
      cb->targets = NULL;
      cb->n_targets = 0;
      cb->provider = NULL;
      if (cb->destroy)
        cb->destroy (cb->data);
      cb->data = NULL;
      cb->destroy = NULL;
    }
}

struct _RetrievalInfo
{
  GdkClipboardX11 *clipboard;
  Atom     target;               /* Form of selection that we requested */
  guint32  idle_time;            /* Number of seconds since we last heard
                                   from selection owner */
  guchar  *buffer;               /* Buffer in which to accumulate results */
  gint     length;               /* Current offset in buffer, -1 indicates
                                   not yet started */
  guint32  notify_time;          /* Timestamp from SelectionNotify */
  guint    timeout;

  GAsyncReadyCallback callback; 
  gpointer            user_data;
};

static void
selection_retrieval_complete (RetrievalInfo *info)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (info->clipboard),
                                   info->callback,
                                   info->user_data,
                                   NULL);
  g_object_set_data (G_OBJECT (res), "target", GSIZE_TO_POINTER ((gsize)info->target));
  g_simple_async_result_complete (res);
  g_object_unref (res);
}

static RetrievalInfo *
find_info (GdkClipboardX11 *cb,
           Atom             target)
{
  RetrievalInfo *info = NULL;
  GList *l;

  for (l = cb->retrievals; l; l = l->next)
    {
      info = l->data;
      if (info->target == target)
        return info;
    }

  return NULL;
}

static gboolean
selection_retrieval_timeout (gpointer data)
{
  RetrievalInfo *info = data;

  if (find_info (info->clipboard, info->target) == info)
    {
      info->idle_time++;

      if (info->idle_time <= IDLE_ABORT_TIME)
        return G_SOURCE_CONTINUE;

      g_free (info->buffer);
      info->buffer = NULL;
      selection_retrieval_complete (info);
    }

  return G_SOURCE_REMOVE;
}

static void
gdk_clipboard_x11_get_contents_async (GdkClipboardX11     *cb,
                                      GCancellable        *cancellable,
                                      Atom                 target,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  RetrievalInfo *info;

  info = find_info (cb, target);
  if (info)
    return; /* FIXME: do multiple callbacks */

  info = g_new0 (RetrievalInfo, 1);

  info->clipboard = cb;
  info->target = target;
  info->idle_time = 0;
  info->buffer = NULL;
  info->length = -1;
  info->callback = callback;
  info->user_data = user_data;
  info->timeout = gdk_threads_add_timeout (1000, (GSourceFunc)selection_retrieval_timeout, info);
  g_source_set_name_by_id (info->timeout, "[gdk] selection_retrieval_timeout");

  cb->retrievals = g_list_prepend (cb->retrievals, info);

  XConvertSelection (cb->xdisplay, cb->xselection, target,
                     target, cb->xowner, get_timestamp (cb));
}

static GBytes *
gdk_clipboard_x11_get_contents_finish (GdkClipboardX11  *cb,
                                       GAsyncResult     *res,
                                       Atom              target,
                                       GError          **error)
{
  GBytes *bytes;
  RetrievalInfo *info;

  info = find_info (cb, target);
  if (!info)
    return NULL;

  if (info->buffer)
    bytes = g_bytes_new_static (info->buffer, info->length);
  else
    bytes = NULL;

  g_source_remove (info->timeout);
  cb->retrievals = g_list_remove (cb->retrievals, info);
  g_free (info);
  
  return bytes;
}

static void
gdk_clipboard_x11_get_text_async (GdkClipboard        *clipboard,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GdkClipboardX11 *cb = GDK_CLIPBOARD_X11 (clipboard);

  gdk_clipboard_x11_get_contents_async (cb, cancellable, utf8_string_atom, callback, user_data);
}

static gchar *
gdk_clipboard_x11_get_text_finish (GdkClipboard  *clipboard,
                                   GAsyncResult  *res,
                                   GError       **error)
{
  GdkClipboardX11 *cb = GDK_CLIPBOARD_X11 (clipboard);
  gchar *text = NULL;
  GBytes *bytes;

  bytes = gdk_clipboard_x11_get_contents_finish (cb, res, utf8_string_atom, error);
  if (bytes)
    {
      text = (gchar*)g_bytes_get_data (bytes, NULL);
      g_bytes_unref (bytes);     
    }

  return text;
}

static void
gdk_clipboard_x11_get_targets_async (GdkClipboardX11     *cb,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  gdk_clipboard_x11_get_contents_async (cb, cancellable, targets_atom, callback, user_data);
}

static Atom *
gdk_clipboard_x11_get_targets_finish (GdkClipboardX11  *cb,
                                      GAsyncResult     *res,
                                      gsize            *n_targets,
                                      GError          **error)
{
  GBytes *bytes;
  Atom *targets;
  gsize len;

  bytes = gdk_clipboard_x11_get_contents_finish (cb, res, targets_atom, error);
  if (bytes)
    {
      targets = (Atom*)g_bytes_get_data (bytes, &len);
      g_bytes_unref (bytes);     
    }
  else
    {
      targets = NULL;
      len = 0;
    }

  *n_targets = len / sizeof (Atom);

  return targets; 
}

static void
gdk_clipboard_x11_set_text (GdkClipboard *clipboard,
                            const gchar  *text)
{
  GdkClipboardX11 *cb = GDK_CLIPBOARD_X11 (clipboard);

  if (claim_selection (cb))
    {
      clear_data (cb);
      cb->text = g_strdup (text);
      cb->n_targets = 10;
      cb->targets = g_new (Atom, cb->n_targets);
      cb->targets[0] = targets_atom;
      cb->targets[1] = multiple_atom;
      cb->targets[2] = timestamp_atom;
      cb->targets[3] = utf8_string_atom;
      cb->targets[4] = string_atom;
      cb->targets[5] = text_atom;
      cb->targets[6] = ctext_atom;
      cb->targets[7] = text_plain_atom;
      cb->targets[8] = text_plain_utf8_atom;
      cb->targets[9] = text_plain_locale_atom;
      _gdk_clipboard_set_available_content (clipboard, TEXT_CONTENT, NULL);
    }
}

static void
gdk_clipboard_x11_get_image_async (GdkClipboard        *clipboard,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GdkClipboardX11 *cb = GDK_CLIPBOARD_X11 (clipboard);

  gdk_clipboard_x11_get_contents_async (cb, cancellable, image_png_atom, callback, user_data);
}

static GdkPixbuf *
gdk_clipboard_x11_get_image_finish (GdkClipboard  *clipboard,
                                    GAsyncResult  *res,
                                    GError       **error)
{
  GdkClipboardX11 *cb = GDK_CLIPBOARD_X11 (clipboard);
  GdkPixbuf *pixbuf = NULL;
  GBytes *bytes;
  GdkPixbufLoader *loader;

  bytes = gdk_clipboard_x11_get_contents_finish (cb, res, image_png_atom, error);
  if (bytes)
    {
      loader = gdk_pixbuf_loader_new ();
      if (gdk_pixbuf_loader_write_bytes (loader, bytes, error) &&
          gdk_pixbuf_loader_close (loader, error))
        pixbuf = g_object_ref (gdk_pixbuf_loader_get_pixbuf (loader));
      g_object_unref (loader);
      g_bytes_unref (bytes);     
    }

  return pixbuf;
}

static void
gdk_clipboard_x11_set_image (GdkClipboard *clipboard,
                             GdkPixbuf    *pixbuf)
{
  GdkClipboardX11 *cb = GDK_CLIPBOARD_X11 (clipboard);

  if (claim_selection (cb))
    {
      clear_data (cb);
      cb->pixbuf = g_object_ref (pixbuf);
      cb->n_targets = 4;
      cb->targets = g_new (Atom, cb->n_targets);
      cb->targets[0] = targets_atom;
      cb->targets[1] = multiple_atom;
      cb->targets[2] = timestamp_atom;
      cb->targets[3] = image_png_atom;
      _gdk_clipboard_set_available_content (clipboard, IMAGE_CONTENT, NULL);
    }
}

static void
gdk_clipboard_x11_get_data_async (GdkClipboard        *clipboard,
                                  const gchar         *content_type,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GdkClipboardX11 *cb = GDK_CLIPBOARD_X11 (clipboard);
  Atom target;

  target = XInternAtom (cb->xdisplay, content_type, FALSE);
  gdk_clipboard_x11_get_contents_async (cb, cancellable, target, callback, user_data);
}

static GInputStream *
gdk_clipboard_x11_get_data_finish (GdkClipboard  *clipboard,
                                   GAsyncResult  *res,
                                   GError       **error)
{
  GdkClipboardX11 *cb = GDK_CLIPBOARD_X11 (clipboard);
  GInputStream *is = NULL;
  GBytes *bytes;
  Atom target;

  target = (Atom)GPOINTER_TO_SIZE (g_object_get_data (G_OBJECT (res), "target"));
  bytes = gdk_clipboard_x11_get_contents_finish (cb, res, target, error);
  if (bytes)
    {
      is = g_memory_input_stream_new_from_bytes (bytes);
      g_bytes_unref (bytes);     
    }

  return is;
}

static void
gdk_clipboard_x11_set_data (GdkClipboard          *clipboard,
                            const gchar          **content_types,
                            GdkClipboardProvider   provider,
                            gpointer               data,
                            GDestroyNotify         destroy)
{
  GdkClipboardX11 *cb = GDK_CLIPBOARD_X11 (clipboard);
  gint i;

  if (claim_selection (cb))
    {
      clear_data (cb);
      cb->n_targets = g_strv_length ((gchar**)content_types) + 3;
      cb->targets = g_new (Atom, cb->n_targets);
      cb->targets[0] = targets_atom;
      cb->targets[1] = multiple_atom;
      cb->targets[2] = timestamp_atom;
      for (i = 0; content_types[i]; i++)
        cb->targets[i+3] = XInternAtom (cb->xdisplay, content_types[i], FALSE);
      cb->provider = provider;
      cb->data = data;
      cb->destroy = destroy;
      _gdk_clipboard_set_available_content (clipboard, OTHER_CONTENT, content_types);
    }
}

static void
gdk_clipboard_x11_clear (GdkClipboard *clipboard)
{
  GdkClipboardX11 *cb = GDK_CLIPBOARD_X11 (clipboard);

  clear_data (cb);
  drop_selection (cb);
}

static void
gdk_clipboard_x11_init (GdkClipboardX11 *clipboard)
{
}

static void
gdk_clipboard_x11_finalize (GObject *object)
{
  GdkClipboardX11 *cb = GDK_CLIPBOARD_X11 (object);

  clear_data (cb);
  gdk_window_destroy (cb->owner);

  G_OBJECT_CLASS (gdk_clipboard_x11_parent_class)->finalize (object);
}

static void
gdk_clipboard_x11_class_init (GdkClipboardX11Class *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (class);

  object_class->finalize = gdk_clipboard_x11_finalize;

  clipboard_class->get_text_async = gdk_clipboard_x11_get_text_async;
  clipboard_class->get_text_finish = gdk_clipboard_x11_get_text_finish;
  clipboard_class->set_text = gdk_clipboard_x11_set_text;
  clipboard_class->get_image_async = gdk_clipboard_x11_get_image_async;
  clipboard_class->get_image_finish = gdk_clipboard_x11_get_image_finish;
  clipboard_class->set_image = gdk_clipboard_x11_set_image;
  clipboard_class->get_data_async = gdk_clipboard_x11_get_data_async;
  clipboard_class->get_data_finish = gdk_clipboard_x11_get_data_finish;
  clipboard_class->set_data = gdk_clipboard_x11_set_data;
  clipboard_class->clear = gdk_clipboard_x11_clear;
}

GdkClipboardX11 *
gdk_clipboard_x11_new (GdkDisplay  *display,
                       const gchar *selection)
{
  GdkClipboardX11 *cb;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkWindow *root;

  cb = (GdkClipboardX11 *) g_object_new (GDK_TYPE_CLIPBOARD_X11, NULL);

  attributes.x = -100;
  attributes.y = -100;
  attributes.width = 10;
  attributes.height = 10;
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.override_redirect = TRUE;
  attributes.event_mask = 0;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

  root = gdk_screen_get_root_window (gdk_display_get_default_screen (display));

  cb->display = display;
  cb->owner = gdk_window_new (root, &attributes, attributes_mask);
  gdk_window_ensure_native (cb->owner);
  cb->xdisplay = GDK_DISPLAY_XDISPLAY (display);
  cb->xowner = gdk_x11_window_get_xid (cb->owner);
  cb->xselection = XInternAtom (cb->xdisplay, selection, FALSE);

  gdk_x11_display_request_selection_notification (display, gdk_x11_xatom_to_atom (cb->xselection));

  init_atoms (cb->xdisplay);

  return cb;
}

gboolean
gdk_clipboard_x11_handle_selection_clear (GdkClipboardX11      *cb,
                                          XSelectionClearEvent *event)
{
  if (!cb)
    return FALSE;

  if (cb->xselection != event->selection || cb->xowner != event->window)
    return FALSE;

  cb->is_owner = FALSE;
  clear_data (cb);
  _gdk_clipboard_set_available_content (GDK_CLIPBOARD (cb), NO_CONTENT, NULL);

  return TRUE;
}

/* Normalize \r and \n into \r\n */
static gchar *
normalize_to_crlf (const gchar *str)
{
  gint len = strlen (str);
  GString *result = g_string_sized_new (len);
  const gchar *p = str;
  const gchar *end = str + len;

  while (p < end)
    {
      if (*p == '\n')
        g_string_append_c (result, '\r');

      if (*p == '\r')
        {
          g_string_append_c (result, *p);
          p++;
          if (p == end || *p != '\n')
            g_string_append_c (result, '\n');
          if (p == end)
            break;
        }

      g_string_append_c (result, *p);
      p++;
    }

  return g_string_free (result, FALSE);
}

static void
send_selection_notify (GdkClipboardX11        *cb,
                       XSelectionRequestEvent *event,
                       gboolean                success)

{
  XSelectionEvent xevent;

  xevent.type = SelectionNotify;
  xevent.serial = 0;
  xevent.send_event = True;
  xevent.requestor = event->requestor;
  xevent.selection = event->selection;
  xevent.target = event->target;
  xevent.time = event->time;
  if (success)
    xevent.property = event->property;
  else
    xevent.property = None;

  _gdk_x11_display_send_xevent (cb->display, xevent.requestor, False, NoEventMask, (XEvent*)&xevent);
}

gboolean
gdk_clipboard_x11_handle_selection_request (GdkClipboardX11        *cb,
                                            XSelectionRequestEvent *event)
{
  GdkClipboardContent content;

  if (!cb)
    return FALSE;

  if (cb->xselection != event->selection ||
      cb->xowner != event->owner ||
      !cb->is_owner)
    return FALSE;

  content = _gdk_clipboard_get_available_content (GDK_CLIPBOARD (cb));

  if (content == NO_CONTENT)
    return FALSE;

  if (event->target == targets_atom)
    {
      XChangeProperty (cb->xdisplay, event->requestor, event->property,
                       XA_ATOM, 32, PropModeReplace,
                       (guchar *)cb->targets, cb->n_targets);
      send_selection_notify (cb, event, TRUE);
      return TRUE;
    }
  else if (event->target == timestamp_atom)
    {
      gulong time = cb->time;
      XChangeProperty (cb->xdisplay, event->requestor, event->property,
                       XA_INTEGER, 32, PropModeReplace, (guchar *)&time, sizeof (time));
      send_selection_notify (cb, event, TRUE);
      return TRUE;
    }

  if (content == TEXT_CONTENT)
    {
      if (event->target == utf8_string_atom)
        {
          XChangeProperty (cb->xdisplay, event->requestor, event->property,
                           utf8_string_atom, 8, PropModeReplace,
                           (guchar *)cb->text, strlen (cb->text));
          send_selection_notify (cb, event, TRUE);
          return TRUE;
        }
      else if (event->target == string_atom)
        {
          gchar *tmp = g_strdup (cb->text);
          gchar *latin1 = gdk_utf8_to_string_target (tmp);

          XChangeProperty (cb->xdisplay, event->requestor, event->property,
                           string_atom, 8, PropModeReplace,
                           (guchar *)latin1, strlen (latin1));
          send_selection_notify (cb, event, TRUE);
          g_free (latin1);
          g_free (tmp);
          return TRUE;
        }
      else if (event->target == ctext_atom ||
               event->target == text_atom)
        {
          gchar *tmp = g_strdup (cb->text);
          guchar *text;
          GdkAtom encoding;
          gint format;
          gint new_length;
          gboolean result = FALSE;

          if (gdk_x11_display_utf8_to_compound_text (cb->display, tmp, &encoding, &format, &text, &new_length))
            {
              XChangeProperty (cb->xdisplay, event->requestor, event->property,
                               gdk_x11_atom_to_xatom (encoding), format, PropModeReplace,
                               (guchar *)text, new_length);
              send_selection_notify (cb, event, TRUE);
              gdk_x11_free_compound_text (text);
              result = TRUE;
            }
          else if (event->target == text_atom)
            {
              gchar *latin1 = gdk_utf8_to_string_target (tmp);

              XChangeProperty (cb->xdisplay, event->requestor, event->property,
                               string_atom, 8, PropModeReplace,
                               (guchar *)latin1, strlen (latin1));
              send_selection_notify (cb, event, TRUE);
              g_free (latin1);

              result = TRUE;
            }
          g_free (tmp);
          return result;
        }
      else if (event->target == text_plain_atom ||
               event->target == text_plain_utf8_atom ||
               event->target == text_plain_locale_atom)
        {
          const gchar *charset = NULL;
          gchar *result;
          GError *error = NULL;

          result = normalize_to_crlf (cb->text);
          if (event->target == text_plain_atom)
            charset = "ASCII";
          else if (event->target == text_plain_locale_atom)
            g_get_charset (&charset);

          if (charset)
            {
              gchar *tmp = result;
              result = g_convert_with_fallback (tmp, -1,
                                                charset, "UTF-8",
                                                NULL, NULL, NULL, &error);
              g_free (tmp);
            }

          if (result)
            {
              XChangeProperty (cb->xdisplay, event->requestor, event->property,
                               event->target, 8, PropModeReplace,
                               (guchar *)result, strlen (result));
              send_selection_notify (cb, event, TRUE);
              g_free (result);
              return TRUE;
            }
        }
    }
  else if (content == IMAGE_CONTENT)
    {
      if (event->target == image_png_atom)
        {
          gchar *str;
          gsize len;
          gboolean result;

          str = NULL;
          result = gdk_pixbuf_save_to_buffer (cb->pixbuf, &str, &len,
                                              "png", NULL,
                                              "compression", "2",
                                              NULL); 
          if (result)
            XChangeProperty (cb->xdisplay, event->requestor, event->property,
                             image_png_atom, 8, PropModeReplace, (guchar*)str, len);
          send_selection_notify (cb, event, TRUE);
          return TRUE;
        }
    }
  else if (content == OTHER_CONTENT)
    {
      GOutputStream *os = NULL;
      gint i;

      for (i = 0; i < cb->n_targets; i++)
        {
          if (cb->targets[i] == event->target)
            {
              os = g_memory_output_stream_new_resizable ();
              cb->provider (GDK_CLIPBOARD (cb), XGetAtomName (cb->xdisplay, event->target), os, cb->data);
              XChangeProperty (cb->xdisplay, event->requestor, event->property,
                               event->target, 8, PropModeReplace,
                               (guchar *)g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (os)),
                               g_memory_output_stream_get_size (G_MEMORY_OUTPUT_STREAM (os)));
              g_object_unref (G_OBJECT (os));
              send_selection_notify (cb, event, TRUE);
              return TRUE;
            }
        }
    }

  send_selection_notify (cb, event, FALSE);
  return FALSE;
}

static gint
get_selection_property (Display  *display,
                        Window    owner,
                        Atom      property,
                        guchar  **ret_data,
                        Atom     *ret_type,
                        gint     *ret_format)
{
  gulong nitems;
  gulong nbytes;
  gulong length = 0;
  Atom prop_type;
  gint prop_format;
  guchar *t = NULL;
  gint i;

  if (XGetWindowProperty (display, owner, property,
                          0, 0x1FFFFFFF, False,
                          AnyPropertyType, &prop_type, &prop_format,
                          &nitems, &nbytes, &t) != Success)
    goto err;

  if (prop_type != None)
    {
      if (prop_type == XA_ATOM)
        {
          if (prop_format != 32)
            goto err;

          length = sizeof (Atom) * nitems;
          *ret_data = g_malloc (length);
          for (i = 0; i < nitems; i++)
            ((Atom*)*ret_data)[i] = ((Atom*)t)[i];
          *ret_type = XA_ATOM;
          *ret_format = 32;
        }
      else
        {
          switch (prop_format)
            {
            case 8:
              length = nitems;
              break;
            case 16:
              length = sizeof (short) * nitems;
              break;
            case 32:
              length = sizeof (long) * nitems;
              break;
            default:
              g_assert_not_reached ();
              break;
            }

          *ret_data = g_malloc (length + 1);
          memcpy (*ret_data, t, length);
          (*ret_data)[length] = 0;
          *ret_type = prop_type;
          *ret_format = prop_format;
        }
    }

  XFree (t);

  return length;

err:
  *ret_data = NULL;
  *ret_type = None;
  *ret_format = 0;

  return 0;
}

gboolean
gdk_clipboard_x11_handle_selection_notify (GdkClipboardX11 *cb,
                                           XSelectionEvent *event)
{
  guchar *buffer = NULL;
  Atom type = None;
  gint format;
  gint length;
  RetrievalInfo *info;

  if (!cb)
    return FALSE;

  if (cb->xselection != event->selection)
    return FALSE;

  info = find_info (cb, event->target);
  if (!info)
    return FALSE;

  if (event->property != None)
    length = get_selection_property (cb->xdisplay, cb->xowner, event->property, &buffer, &type, &format);

  if (event->property == None || buffer == NULL)
    {
      g_free (info->buffer);
      info->buffer = NULL;
      info->length = 0;
      selection_retrieval_complete (info);
      return TRUE;
    }

  if (type == incr_atom)
    {
      /* FIXME: handle incr */
      return TRUE;
    }

  info->buffer = buffer;
  info->length = length;
  selection_retrieval_complete (info);

  return TRUE;
}

static void
targets_received (GObject      *source,
                  GAsyncResult *res,
                  gpointer      data)
{
  GdkClipboardX11 *cb = GDK_CLIPBOARD_X11 (source);
  Atom *targets;
  gsize n_targets = 0;
  GdkClipboardContent content;
  gchar **content_types;
  gint i, j;

  content = NO_CONTENT;

  targets = gdk_clipboard_x11_get_targets_finish (cb, res, &n_targets, NULL);    
  content_types = g_new0 (gchar*, n_targets + 1);
  for (i = 0, j = 0; i < n_targets; i++)
    {
      if (targets[i] == utf8_string_atom ||
          targets[i] == string_atom ||
          targets[i] == text_atom ||
          targets[i] == ctext_atom ||
          targets[i] == text_plain_atom ||
          targets[i] == text_plain_atom ||
          targets[i] == text_plain_utf8_atom ||
          targets[i] == text_plain_locale_atom)
        {
          content |= TEXT_CONTENT;
        }
      else if (targets[i] == image_png_atom)
        {
          content |= IMAGE_CONTENT;
        }
      else if (targets[i] == timestamp_atom ||
               targets[i] == targets_atom ||
               targets[i] == multiple_atom ||
               targets[i] == XInternAtom (cb->xdisplay, "SAVE_TARGETS", FALSE))
        {
          continue;
        }
      else if (targets[i] != None)
        {
          content |= OTHER_CONTENT;
          content_types[j++] = XGetAtomName (cb->xdisplay, targets[i]);
        }
    }
  g_free (targets);

  _gdk_clipboard_set_available_content (GDK_CLIPBOARD (cb), content, (const gchar**)content_types);
  for (j = 0; content_types[j]; j++)
    XFree (content_types[j]);
  g_free (content_types);
}

gboolean
gdk_clipboard_x11_handle_selection_owner_change (GdkClipboardX11 *cb,
                                                 XEvent          *xevent)
{
  XFixesSelectionNotifyEvent *event = (XFixesSelectionNotifyEvent *)xevent;

  if (!cb)
    return FALSE;

  if (cb->xselection != event->selection)
   return FALSE;

  if (event->owner != cb->xowner)
    {
      _gdk_clipboard_set_available_content (GDK_CLIPBOARD (cb), NO_CONTENT, NULL);
      gdk_clipboard_x11_get_targets_async (cb, NULL, targets_received, NULL);
    }

  return TRUE;
}
