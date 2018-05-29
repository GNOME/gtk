/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2001 Archaeopteryx Software Inc.
 * Copyright (C) 1998-2002 Tor Lillqvist
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include <string.h>

#include <io.h>
#include <fcntl.h>

/*
 * Support for OLE-2 drag and drop added at Archaeopteryx Software, 2001
 * For more information, do not contact Stephan R.A. Deibel (sdeibel@archaeopteryx.com),
 * because the code went through multiple modifications since then.
 *
 * Notes on the implementation:
 *
 * Source drag context, IDragSource and IDataObject for it are created
 * (almost) simultaneously, whereas target drag context and IDropTarget
 * are separated in time - IDropTarget is created when a window is made
 * to accept drops, while target drag context is created when a dragging
 * cursor enters the window and is destroyed when that cursor leaves
 * the window.
 *
 * There's a mismatch between data types supported by W32 (W32 formats)
 * and by GTK+ (GDK contentformats).
 * To account for it the data is transmuted back and forth. There are two
 * main points of transmutation:
 * * GdkWin32HDATAOutputStream: transmutes GTK+ data to W32 data
 * * GdkWin32DropContext: transmutes W32 data to GTK+ data
 *
 * There are also two points where data formats are considered:
 * * When source drag context is created, it gets a list of GDK contentformats
 *   that it supports, these are matched to the W32 formats they
 *   correspond to (possibly with transmutation). New W32 formats for
 *   GTK+-specific contentformats are also created here (see below).
 * * When target drop context is created, it queries the IDataObject
 *   for the list of W32 formats it supports and matches these to
 *   corresponding GDK contentformats that it will be able to provide
 *   (possibly with transmutation) later. Missing GDK contentformats for
 *   W32-specific formats are also created here (see below).
 *
 * W32 formats are integers (CLIPFORMAT), while GTK+ contentformats
 * are mime/type strings, and cannot be used interchangeably.
 *
 * To accommodate advanced GTK+ applications the code allows them to
 * register drop targets that accept W32 data formats, and to register
 * drag sources that provide W32 data formats. To do that they must
 * register with the mime/type "application/x.windows.ZZZ", where
 * ZZZ is the string name of the format in question
 * (for example, "Shell IDList Array") or, for unnamed pre-defined
 * formats, register with the stringified constant name of the format
 * in question (for example, "CF_UNICODETEXT").
 * If such contentformat is accepted/provided, GDK will not try to
 * transmute it to/from something else. Otherwise GDK will do the following
 * transmutation:
 * * If GTK+ application provides image/png, image/gif or image/jpeg,
 *   GDK will claim to also provide "PNG", "GIF" or "JFIF" respectively,
 *   and will pass these along verbatim.
 * * If GTK+ application provides any GdkPixbuf-compatible contentformat,
 *   GDK will also offer "PNG" and CF_DIB W32 formats.
 * * If GTK+ application provides text/plain;charset=utf8, GDK will also offer
 *   CF_UNICODETEXT (UTF-16-encoded) and CF_TEXT (encoded with thread-
 *   and locale-depenant codepage), and will do the conversion when such
 *   data is requested.
 * * If GTK+ application accepts image/png, image/gif or image/jpeg,
 *   GDK will claim to also accept "PNG", "GIF" or "JFIF" respectively,
 *   and will pass these along verbatim.
 * * If GTK+ application accepts image/bmp, GDK will
 *   claim to accept CF_DIB W32 format, and will convert
 *   it, changing the header, when such data is provided.
 * * If GTK+ application accepts text/plain;charset=utf8, GDK will
 *   claim to accept CF_UNICODETEXT and CF_TEXT, and will do
 *   the conversion when such data is provided.
 * * If GTK+ application accepts text/uri-list, GDK will
 *   claim to accept "Shell IDList Array", and will do the
 *   conversion when such data is provided.
 *
 * Currently the conversion from text/uri-list to "Shell IDList Array" is not
 * implemented, so it's not possible to drag & drop files from GTK+
 * applications to non-GTK+ applications the same way one can drag files
 * from Windows Explorer.
 *
 * To increase inter-GTK compatibility, GDK will register GTK+-specific
 * formats by their mime/types, as-is (i.e "text/plain;charset=utf-8", for example).
 * That way two GTK+ applications can exchange data in their native formats
 * (both well-known ones, such as text/plain;charset=utf8, and special,
 * known only to specific applications). This will work just
 * fine as long as both applications agree on what kind of data is stored
 * under such format exactly.
 *
 * Note that clipboard format space is limited, there can only be 16384
 * of them for a particular user session. Therefore it is highly inadvisable
 * to create and register such formats out of the whole cloth, dynamically.
 * If more flexibility is needed, register one format that has some
 * internal indicators of the kind of data it contains, then write the application
 * in such a way that it requests the data and inspects its header before deciding
 * whether to accept it or not. For details see GTK+ drag & drop documentation
 * on the "drag-motion" and "drag-data-received" signals.
 *
 * How managed DnD works:
 * GTK widget detects a drag gesture and calls
 * S: gdk_drag_begin_from_point() -> backend:drag_begin()
 * which creates the source drag context and the drag window,
 * and grabs the pointing device. GDK layer adds the context
 * to a list of currently-active contexts.
 *
 * From that point forward the context gets any events emitted
 * by GDK, and can prevent these events from going anywhere else.
 * They are all handled in
 * S: gdk_drag_context_handle_source_event() -> backend:handle_event()
 * (except for wayland backend - it doesn't have that function).
 *
 * That function catches the following events:
 * GDK_MOTION_NOTIFY
 * GDK_BUTTON_RELEASE
 * GDK_KEY_PRESS
 * GDK_KEY_RELEASE
 * GDK_GRAB_BROKEN
 *
 * GDK_MOTION_NOTIFY is emitted by the backend in response to
 * mouse movement.
 * Drag context handles it by calling a bunch of functions to
 * determine the state of the drag actions from the keys being
 * pressed, finding the drag window (another backend function
 * routed through GDK layer) and finally calls
 * S: gdk_drag_motion -> backend:drag_motion()
 * to notify the backend (i.e. itself) that the drag cursor
 * moved.
 * The response to that is to move the drag window and
 * do various bookkeeping.
 * W32: OLE2 protocol does nothing (other than moving the
 * drag window) in response to this, as all the functions
 * that GDK could perform here are already handled by the
 * OS driving the DnD process via DoDragDrop() call.
 * The LOCAL protocol, on the other hande, does a lot,
 * similar to what X11 backend does with XDND - it sends
 * GDK_DRAG_LEAVE and GDK_DRAG_ENTER, emits GDK_DRAG_MOTION.
 *
 * GDK_BUTTON_RELEASE checks the
 * released button - if it's the button that was used to
 * initiate the drag, the "drop-performed" signal is emitted,
 * otherwise the drag is cancelled.
 *
 * GDK_KEY_PRESS and GDK_KEY_RELEASE handler does exactly the same thing as
 * GDK_MOTION_NOTIFY handler, but only after it checks the pressed
 * keys to emit "drop-performed" signal (on Space, Enter etc),
 * cancel the drag (on Escape) or move the drag cursor (arrow keys).
 *
 * GDK_GRAB_BROKEN handler cancels the drag for most broken grabs
 * (except for some special cases where the backend itself does
 *  temporary grabs as part of DnD, such as changing the cursor).
 *
 * GDK_DRAG_ENTER, GDK_DRAG_LEAVE, GDK_DRAG_MOTION and GDK_DROP_START
 * events are emitted when
 * the OS notifies the process about these things happening.
 * For X11 backend that is done in Xdnd event filters,
 * for W32 backend this is done in IDropSource/IDropTarget
 * object methods for the OLE2 protocol, whereas for the
 * LOCAL protocol these events are emitted only by GDK itself
 * (with the exception of WM_DROPFILES message, which causes
 *  GDK to create a drop context and then immediately finish
 *  the drag, providing the list of files it got from the message).
 * 
 */

/* The mingw.org compiler does not export GUIDS in it's import library. To work
 * around that, define INITGUID to have the GUIDS declared. */
#if defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
#define INITGUID
#endif

/* For C-style COM wrapper macros */
#define COBJMACROS

#include "gdkdnd.h"
#include "gdkproperty.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"
#include "gdkwin32.h"
#include "gdkwin32dnd.h"
#include "gdkdisplayprivate.h"
#include "gdk/gdkdndprivate.h"
#include "gdkwin32dnd-private.h"
#include "gdkdisplay-win32.h"
#include "gdkdeviceprivate.h"
#include "gdkhdataoutputstream-win32.h"

#include <ole2.h>

#include <shlobj.h>
#include <shlguid.h>
#include <objidl.h>
#include "gdkintl.h"

#include <gdk/gdk.h>
#include <glib/gstdio.h>

/* Just to avoid calling RegisterWindowMessage() every time */
static UINT thread_wakeup_message;

typedef struct
{
  IDropSource                     ids;
  IDropSourceNotify               idsn;
  gint                            ref_count;
  GdkDragContext                 *context;

  /* These are thread-local
   * copies of the similar fields from GdkWin32DragContext
   */
  GdkWin32DragContextUtilityData  util_data;

  /* Cached here, so that we don't have to look in
   * the context every time.
   */
  HWND                            source_window_handle;
  guint                           scale;

  /* We get this from the OS via IDropSourceNotify and pass it to the
   * main thread.
   */
  HWND                            dest_window_handle;
} source_drag_context;

typedef struct {
  IDataObject                     ido;
  int                             ref_count;
  GdkDragContext                 *context;
  GArray                         *formats;
} data_object;

typedef struct {
  IEnumFORMATETC                  ief;
  int                             ref_count;
  int                             ix;
  GArray                         *formats;
} enum_formats;

typedef enum _GdkWin32DnDThreadQueueItemType GdkWin32DnDThreadQueueItemType;

enum _GdkWin32DnDThreadQueueItemType
{
  GDK_WIN32_DND_THREAD_QUEUE_ITEM_GIVE_FEEDBACK = 1,
  GDK_WIN32_DND_THREAD_QUEUE_ITEM_DRAG_INFO = 2,
  GDK_WIN32_DND_THREAD_QUEUE_ITEM_DO_DRAG_DROP = 3,
  GDK_WIN32_DND_THREAD_QUEUE_ITEM_GET_DATA = 4,
  GDK_WIN32_DND_THREAD_QUEUE_ITEM_UPDATE_DRAG_STATE = 5,
};

typedef struct _GdkWin32DnDThreadQueueItem GdkWin32DnDThreadQueueItem;

struct _GdkWin32DnDThreadQueueItem
{
  GdkWin32DnDThreadQueueItemType  item_type;

  /* This is used by the DnD thread to communicate the identity
   * of the drag context to the main thread. This is thread-safe
   * because DnD thread holds a reference to the context.
   */
  gpointer                        opaque_context;
};

typedef struct _GdkWin32DnDThreadDoDragDrop GdkWin32DnDThreadDoDragDrop;

/* This is used both to signal the DnD thread that it needs
 * to call DoDragDrop(), *and* to signal the main thread
 * that the DoDragDrop() call returned.
 */
struct _GdkWin32DnDThreadDoDragDrop
{
  GdkWin32DnDThreadQueueItem  base;

  source_drag_context        *src_context;
  data_object                *src_object;
  DWORD                       allowed_drop_effects;

  DWORD                       received_drop_effect;
  HRESULT                     received_result;
};

typedef struct _GdkWin32DnDThreadGetData GdkWin32DnDThreadGetData;

/* This is used both to signal the main thread that the DnD thread
 * needs DnD data, and to give that data to the DnD thread.
 */
struct _GdkWin32DnDThreadGetData
{
  GdkWin32DnDThreadQueueItem  base;

  GdkWin32ContentFormatPair   pair;
  GdkWin32HDataOutputStream  *stream;

  STGMEDIUM                   produced_data_medium;
};

typedef struct _GdkWin32DnDThreadGiveFeedback GdkWin32DnDThreadGiveFeedback;

struct _GdkWin32DnDThreadGiveFeedback
{
  GdkWin32DnDThreadQueueItem base;

  DWORD                      received_drop_effect;
};

typedef struct _GdkWin32DnDThreadDragInfo GdkWin32DnDThreadDragInfo;

struct _GdkWin32DnDThreadDragInfo
{
  GdkWin32DnDThreadQueueItem base;

  BOOL                       received_escape_pressed;
  DWORD                      received_keyboard_mods;
};

typedef struct _GdkWin32DnDThreadUpdateDragState GdkWin32DnDThreadUpdateDragState;

struct _GdkWin32DnDThreadUpdateDragState
{
  GdkWin32DnDThreadQueueItem base;

  gpointer                       opaque_ddd;
  GdkWin32DragContextUtilityData produced_util_data;
};

typedef struct _GdkWin32DnDThread GdkWin32DnDThread;

struct _GdkWin32DnDThread
{
  /* We receive instructions from the main thread in this queue */
  GAsyncQueue *input_queue;

  /* We can't peek the queue or "unpop" queue items,
   * so the items that we can't act upon (yet) got
   * to be stored *somewhere*.
   */
  GList       *dequeued_items;

  source_drag_context *src_context;
  data_object         *src_object;
};

/* The code is much more secure if we don't rely on the OS to keep
 * this around for us.
 */
static GdkWin32DnDThread *dnd_thread_data = NULL;

static gboolean
dnd_queue_is_empty ()
{
  return g_atomic_int_get (&_win32_clipdrop->dnd_queue_counter) == 0;
}

static void
decrement_dnd_queue_counter ()
{
  g_atomic_int_dec_and_test (&_win32_clipdrop->dnd_queue_counter);
}

static void
increment_dnd_queue_counter ()
{
  g_atomic_int_inc (&_win32_clipdrop->dnd_queue_counter);
}

static void
free_queue_item (GdkWin32DnDThreadQueueItem *item)
{
  GdkWin32DnDThreadGetData *getdata;

  switch (item->item_type)
    {
    case GDK_WIN32_DND_THREAD_QUEUE_ITEM_DO_DRAG_DROP:
      /* Don't unref anything, it's all done in the main thread,
       * when it receives a DoDragDrop reply.
       */
      break;
    case GDK_WIN32_DND_THREAD_QUEUE_ITEM_UPDATE_DRAG_STATE:
    case GDK_WIN32_DND_THREAD_QUEUE_ITEM_GIVE_FEEDBACK:
    case GDK_WIN32_DND_THREAD_QUEUE_ITEM_DRAG_INFO:
      break;
    case GDK_WIN32_DND_THREAD_QUEUE_ITEM_GET_DATA:
      getdata = (GdkWin32DnDThreadGetData *) item;

      switch (getdata->produced_data_medium.tymed)
        {
        case TYMED_FILE:
        case TYMED_ISTREAM:
        case TYMED_ISTORAGE:
        case TYMED_GDI:
        case TYMED_MFPICT:
        case TYMED_ENHMF:
          g_critical ("Unsupported STGMEDIUM type");
          break;
        case TYMED_NULL:
          break;
        case TYMED_HGLOBAL:
          GlobalFree (getdata->produced_data_medium.hGlobal);
          break;
        }
    }

  g_free (item);
}

static gboolean
process_dnd_queue (gboolean                   timed,
                   guint64                    end_time,
                   GdkWin32DnDThreadGetData  *getdata_check)
{
  GdkWin32DnDThreadQueueItem *item;
  GdkWin32DnDThreadUpdateDragState *updatestate;
  GdkWin32DnDThreadDoDragDrop *ddd;

  while (TRUE)
    {
      if (timed)
        {
          guint64 current_time = g_get_monotonic_time ();

          if (current_time >= end_time)
            break;

          item = g_async_queue_timeout_pop (dnd_thread_data->input_queue, end_time - current_time);
        }
      else
        {
          item = g_async_queue_try_pop (dnd_thread_data->input_queue);
        }

      if (item == NULL)
        break;

      decrement_dnd_queue_counter ();

      switch (item->item_type)
        {
        case GDK_WIN32_DND_THREAD_QUEUE_ITEM_DO_DRAG_DROP:
          /* We don't support more than one DnD at a time */
          free_queue_item (item);
          break;
        case GDK_WIN32_DND_THREAD_QUEUE_ITEM_UPDATE_DRAG_STATE:
          updatestate = (GdkWin32DnDThreadUpdateDragState *) item;
          ddd = (GdkWin32DnDThreadDoDragDrop *) updatestate->opaque_ddd;
          ddd->src_context->util_data = updatestate->produced_util_data;
          free_queue_item (item);
          break;
        case GDK_WIN32_DND_THREAD_QUEUE_ITEM_GET_DATA:
          if (item == (GdkWin32DnDThreadQueueItem *) getdata_check)
            return TRUE;

          free_queue_item (item);
          break;
        case GDK_WIN32_DND_THREAD_QUEUE_ITEM_GIVE_FEEDBACK:
        case GDK_WIN32_DND_THREAD_QUEUE_ITEM_DRAG_INFO:
          g_assert_not_reached ();
        }
    }

  return FALSE;
}

static gboolean
do_drag_drop_response (gpointer user_data)
{
  GdkWin32DnDThreadDoDragDrop *ddd = (GdkWin32DnDThreadDoDragDrop *) user_data;
  HRESULT hr = ddd->received_result;
  GdkDragContext *context = GDK_DRAG_CONTEXT (ddd->base.opaque_context);
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  gpointer table_value = g_hash_table_lookup (clipdrop->active_source_drags, context);

  /* This just verifies that we got the right context,
   * we don't need the ddd struct itself.
   */
  if (ddd == table_value)
    {
      GDK_NOTE (DND, g_print ("DoDragDrop returned %s with effect %lu\n",
                              (hr == DRAGDROP_S_DROP ? "DRAGDROP_S_DROP" :
                               (hr == DRAGDROP_S_CANCEL ? "DRAGDROP_S_CANCEL" :
                                (hr == E_UNEXPECTED ? "E_UNEXPECTED" :
                                 g_strdup_printf ("%#.8lx", hr)))), ddd->received_drop_effect));

      GDK_WIN32_DRAG_CONTEXT (context)->drop_failed = !(SUCCEEDED (hr) || hr == DRAGDROP_S_DROP);

      /* We used to delete the selection here,
       * now GTK does that automatically in response to 
       * the "dnd-finished" signal,
       * if the operation was successful and was a move.
       */
      GDK_NOTE (DND, g_print ("gdk_dnd_handle_drop_finihsed: 0x%p\n",
                              context));

      g_signal_emit_by_name (context, "dnd-finished");
      gdk_drag_drop_done (context, !(GDK_WIN32_DRAG_CONTEXT (context))->drop_failed);
    }
  else
    {
      if (!table_value)
        g_critical ("Did not find context 0x%p in the active contexts table", context);
      else
        g_critical ("Found context 0x%p in the active contexts table, but the record doesn't match (0x%p != 0x%p)", context, ddd, table_value);
    }

  /* 3rd parties could keep a reference to this object,
   * but we won't keep the context alive that long.
   * Neutralize it (attempts to get its data will fail)
   * by nullifying the context pointer (it doesn't hold
   * a reference, so no unreffing).
   */
  ddd->src_object->context = NULL;

  IDropSource_Release (&ddd->src_context->ids);
  IDataObject_Release (&ddd->src_object->ido);

  g_hash_table_remove (clipdrop->active_source_drags, context);
  free_queue_item (&ddd->base);

  return G_SOURCE_REMOVE;
}

static void
received_drag_context_data (GObject      *context,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  GError *error = NULL;
  GdkWin32DnDThreadGetData *getdata = (GdkWin32DnDThreadGetData *) user_data;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();

  if (!gdk_drag_context_write_finish (GDK_DRAG_CONTEXT (context), result, &error))
    {
      HANDLE handle;
      gboolean is_hdata;

      GDK_NOTE (DND, g_printerr ("%p: failed to write HData-backed stream: %s\n", context, error->message));
      g_error_free (error);
      g_output_stream_close (G_OUTPUT_STREAM (getdata->stream), NULL, NULL);
      handle = gdk_win32_hdata_output_stream_get_handle (getdata->stream, &is_hdata);

      if (is_hdata)
        API_CALL (GlobalFree, (handle));
      else
        API_CALL (CloseHandle, (handle));
    }
  else
    {
      g_output_stream_close (G_OUTPUT_STREAM (getdata->stream), NULL, NULL);
      getdata->produced_data_medium.tymed = TYMED_HGLOBAL;
      getdata->produced_data_medium.hGlobal = gdk_win32_hdata_output_stream_get_handle (getdata->stream, NULL);
    }

  g_clear_object (&getdata->stream);
  increment_dnd_queue_counter ();
  g_async_queue_push (clipdrop->dnd_queue, getdata);
  API_CALL (PostThreadMessage, (clipdrop->dnd_thread_id, thread_wakeup_message, 0, 0));
}

static gboolean
get_data_response (gpointer user_data)
{
  GdkWin32DnDThreadGetData *getdata = (GdkWin32DnDThreadGetData *) user_data;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  GdkDragContext *context = GDK_DRAG_CONTEXT (getdata->base.opaque_context);
  gpointer ddd = g_hash_table_lookup (clipdrop->active_source_drags, context);

  GDK_NOTE (DND, g_print ("idataobject_getdata will request target 0x%p (%s)",
                          getdata->pair.contentformat, getdata->pair.contentformat));

  /* This just verifies that we got the right context,
   * we don't need the ddd struct itself.
   */
  if (ddd)
    {
      GError *error = NULL;
      GOutputStream *stream = gdk_win32_hdata_output_stream_new (&getdata->pair, &error);

      if (stream)
        {
          getdata->stream = GDK_WIN32_HDATA_OUTPUT_STREAM (stream);
          gdk_drag_context_write_async (context,
                                        getdata->pair.contentformat,
                                        stream,
                                        G_PRIORITY_DEFAULT,
                                        NULL,
                                        received_drag_context_data,
                                        getdata);

          return G_SOURCE_REMOVE;
        }
    }

  increment_dnd_queue_counter ();
  g_async_queue_push (clipdrop->dnd_queue, getdata);
  API_CALL (PostThreadMessage, (clipdrop->dnd_thread_id, thread_wakeup_message, 0, 0));

  return G_SOURCE_REMOVE;
}

static void
do_drag_drop (GdkWin32DnDThreadDoDragDrop *ddd)
{
  HRESULT hr;

  dnd_thread_data->src_object = ddd->src_object;
  dnd_thread_data->src_context = ddd->src_context;

  hr = DoDragDrop (&dnd_thread_data->src_object->ido,
                   &dnd_thread_data->src_context->ids,
                   ddd->allowed_drop_effects,
                   &ddd->received_drop_effect);

  ddd->received_result = hr;

  g_idle_add_full (G_PRIORITY_DEFAULT, do_drag_drop_response, ddd, NULL);
}

gpointer
_gdk_win32_dnd_thread_main (gpointer data)
{
  GAsyncQueue *queue = (GAsyncQueue *) data;
  GdkWin32DnDThreadQueueItem *item;
  MSG msg;
  HRESULT hr;

  g_assert (dnd_thread_data == NULL);

  dnd_thread_data = g_new0 (GdkWin32DnDThread, 1);
  dnd_thread_data->input_queue = queue;

  CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);

  hr = OleInitialize (NULL);

  if (!SUCCEEDED (hr))
    g_error ("OleInitialize failed");

  /* Create a message queue */
  PeekMessage (&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

  thread_wakeup_message = RegisterWindowMessage ("GDK_WORKER_THREAD_WEAKEUP");

  /* Signal the main thread that we're ready.
   * This is the only time the queue works in reverse.
   */
  g_async_queue_push (queue, GUINT_TO_POINTER (GetCurrentThreadId ()));

  while (GetMessage (&msg, NULL, 0, 0))
    {
      if (!dnd_queue_is_empty ())
        {
          while ((item = g_async_queue_try_pop (queue)) != NULL)
            {
              decrement_dnd_queue_counter ();

              if (item->item_type != GDK_WIN32_DND_THREAD_QUEUE_ITEM_DO_DRAG_DROP)
                {
                  free_queue_item (item);
                  continue;
                }

              do_drag_drop ((GdkWin32DnDThreadDoDragDrop *) item);
              API_CALL (PostThreadMessage, (GetCurrentThreadId (), thread_wakeup_message, 0, 0));
              break;
            }
        }

      /* Just to be safe, although this mostly does nothing */
      TranslateMessage (&msg); 
      DispatchMessage (&msg); 
    }

  g_async_queue_unref (queue);
  g_clear_pointer (&dnd_thread_data, g_free);

  OleUninitialize ();
  CoUninitialize ();

  return NULL;
}

/* For the LOCAL protocol */
typedef enum {
  GDK_DRAG_STATUS_DRAG,
  GDK_DRAG_STATUS_MOTION_WAIT,
  GDK_DRAG_STATUS_ACTION_WAIT,
  GDK_DRAG_STATUS_DROP
} GdkDragStatus;

static GList *local_source_contexts;
static GdkDragContext *current_dest_drag = NULL;

static gboolean use_ole2_dnd = TRUE;

static gboolean drag_context_grab (GdkDragContext *context);

G_DEFINE_TYPE (GdkWin32DragContext, gdk_win32_drag_context, GDK_TYPE_DRAG_CONTEXT)

static void
move_drag_surface (GdkDragContext *context,
                   guint           x_root,
                   guint           y_root)
{
  GdkWin32DragContext *context_win32 = GDK_WIN32_DRAG_CONTEXT (context);

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  gdk_surface_move (context_win32->drag_surface,
                    x_root - context_win32->hot_x,
                    y_root - context_win32->hot_y);
  gdk_surface_raise (context_win32->drag_surface);
}

static void
gdk_win32_drag_context_init (GdkWin32DragContext *context)
{
  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  if (!use_ole2_dnd)
    {
      local_source_contexts = g_list_prepend (local_source_contexts, context);
    }
  else
    {
    }

  GDK_NOTE (DND, g_print ("gdk_drag_context_init %p\n", context));
}

static void
gdk_win32_drag_context_finalize (GObject *object)
{
  GdkDragContext *context;
  GdkWin32DragContext *context_win32;
  GdkSurface *drag_surface;

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  GDK_NOTE (DND, g_print ("gdk_drag_context_finalize %p\n", object));

  g_return_if_fail (GDK_IS_WIN32_DRAG_CONTEXT (object));

  context = GDK_DRAG_CONTEXT (object);
  context_win32 = GDK_WIN32_DRAG_CONTEXT (context);

  if (!use_ole2_dnd)
    {
      local_source_contexts = g_list_remove (local_source_contexts, context);

      if (context == current_dest_drag)
        current_dest_drag = NULL;
    }

  g_set_object (&context_win32->ipc_window, NULL);
  drag_surface = context_win32->drag_surface;

  G_OBJECT_CLASS (gdk_win32_drag_context_parent_class)->finalize (object);

  if (drag_surface)
    gdk_surface_destroy (drag_surface);
}

/* Drag Contexts */

static GdkDragContext *
gdk_drag_context_new (GdkDisplay         *display,
                      GdkContentProvider *content,
                      GdkSurface         *source_surface,
                      GdkContentFormats  *formats,
                      GdkDragAction       actions,
                      GdkDevice          *device,
                      GdkDragProtocol     protocol)
{
  GdkWin32DragContext *context_win32;
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);
  GdkDragContext *context;

  context_win32 = g_object_new (GDK_TYPE_WIN32_DRAG_CONTEXT,
                                "device", device ? device : gdk_seat_get_pointer (gdk_display_get_default_seat (display)),
                                "content", content,
                                "formats", formats,
                                NULL);

  context = GDK_DRAG_CONTEXT (context_win32);

  if (win32_display->has_fixed_scale)
    context_win32->scale = win32_display->surface_scale;
  else
    context_win32->scale = _gdk_win32_display_get_monitor_scale_factor (win32_display, NULL, NULL, NULL);

  context->is_source = TRUE;
  g_set_object (&context->source_surface, source_surface);
  gdk_drag_context_set_actions (context, actions, actions);
  context_win32->protocol = protocol;

  gdk_content_formats_unref (formats);

  return context;
}

GdkDragContext *
_gdk_win32_drag_context_find (GdkSurface *source,
                              GdkSurface *dest)
{
  GList *tmp_list = local_source_contexts;
  GdkDragContext *context;

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  while (tmp_list)
    {
      context = (GdkDragContext *)tmp_list->data;

      if (context->is_source &&
          ((source == NULL) || (context->source_surface && (context->source_surface == source))) &&
          ((dest == NULL) || (context->dest_surface && (context->dest_surface == dest))))
        return context;

      tmp_list = tmp_list->next;
    }

  return NULL;
}

#define PRINT_GUID(guid) \
  g_print ("%.08lx-%.04x-%.04x-%.02x%.02x-%.02x%.02x%.02x%.02x%.02x%.02x", \
           ((gulong *)  guid)[0], \
           ((gushort *) guid)[2], \
           ((gushort *) guid)[3], \
           ((guchar *)  guid)[8], \
           ((guchar *)  guid)[9], \
           ((guchar *)  guid)[10], \
           ((guchar *)  guid)[11], \
           ((guchar *)  guid)[12], \
           ((guchar *)  guid)[13], \
           ((guchar *)  guid)[14], \
           ((guchar *)  guid)[15]);

static enum_formats *enum_formats_new (GArray *formats);

GdkDragContext *
_gdk_win32_find_source_context_for_dest_surface (GdkSurface *dest_surface)
{
  GHashTableIter               iter;
  GdkWin32DragContext         *win32_context;
  GdkWin32DnDThreadDoDragDrop *ddd;
  GdkWin32Clipdrop            *clipdrop = _gdk_win32_clipdrop_get ();

  g_hash_table_iter_init (&iter, clipdrop->active_source_drags);

  while (g_hash_table_iter_next (&iter, (gpointer *) &win32_context, (gpointer *) &ddd))
    if (ddd->src_context->dest_window_handle == GDK_SURFACE_HWND (dest_surface))
      return GDK_DRAG_CONTEXT (win32_context);

  return NULL;
}

static GdkDragAction
action_for_drop_effect (DWORD effect)
{
  GdkDragAction action = 0;

  if (effect & DROPEFFECT_MOVE)
    action |= GDK_ACTION_MOVE;
  if (effect & DROPEFFECT_LINK)
    action |= GDK_ACTION_LINK;
  if (effect & DROPEFFECT_COPY)
    action |= GDK_ACTION_COPY;

  return action;
}

static ULONG STDMETHODCALLTYPE
idropsource_addref (LPDROPSOURCE This)
{
  source_drag_context *ctx = (source_drag_context *) This;

  int ref_count = ++ctx->ref_count;

  GDK_NOTE (DND, g_print ("idropsource_addref %p %d\n", This, ref_count));

  return ref_count;
}

typedef struct _GdkWin32DnDEnterLeaveNotify GdkWin32DnDEnterLeaveNotify;

struct _GdkWin32DnDEnterLeaveNotify
{
  gpointer opaque_context;
  HWND     target_window_handle;
};

static gboolean
notify_dnd_enter (gpointer user_data)
{
  GdkWin32DnDEnterLeaveNotify *notify = (GdkWin32DnDEnterLeaveNotify *) user_data;
  GdkDragContext *context = GDK_DRAG_CONTEXT (notify->opaque_context);
  GdkSurface *dest_surface, *dw;

  dw = gdk_win32_handle_table_lookup (notify->target_window_handle);

  if (dw)
    dest_surface = g_object_ref (dw);
  else
    dest_surface = gdk_win32_surface_foreign_new_for_display (gdk_drag_context_get_display (context), notify->target_window_handle);

  g_clear_object (&context->dest_surface);
  context->dest_surface = dest_surface;

  g_free (notify);

  return G_SOURCE_REMOVE;
}

static gboolean
notify_dnd_leave (gpointer user_data)
{
  GdkWin32DnDEnterLeaveNotify *notify = (GdkWin32DnDEnterLeaveNotify *) user_data;
  GdkDragContext *context = GDK_DRAG_CONTEXT (notify->opaque_context);
  GdkSurface *dest_surface, *dw;

  dw = gdk_win32_handle_table_lookup (notify->target_window_handle);

  if (dw)
    {
      dest_surface = gdk_surface_get_toplevel (dw);

      if (dest_surface == context->dest_surface)
        g_clear_object (&context->dest_surface);
      else
        g_warning ("Destination window for handle 0x%p is 0x%p, but context has 0x%p", notify->target_window_handle, dest_surface, context->dest_surface);
    }
  else
    g_warning ("Failed to find destination window for handle 0x%p", notify->target_window_handle);

  g_free (notify);

  return G_SOURCE_REMOVE;
}

static HRESULT STDMETHODCALLTYPE
idropsourcenotify_dragentertarget (IDropSourceNotify *This,
                                   HWND               hwndTarget)
{
  source_drag_context *ctx = (source_drag_context *) (((char *) This) - G_STRUCT_OFFSET (source_drag_context, idsn));
  GdkWin32DnDEnterLeaveNotify *notify;

  if (!dnd_queue_is_empty ())
    process_dnd_queue (FALSE, 0, NULL);

  GDK_NOTE (DND, g_print ("idropsourcenotify_dragentertarget %p (SDC %p) 0x%p\n", This, ctx, hwndTarget));

  ctx->dest_window_handle = hwndTarget;

  notify = g_new0 (GdkWin32DnDEnterLeaveNotify, 1);
  notify->target_window_handle = hwndTarget;
  notify->opaque_context = ctx->context;
  g_idle_add_full (G_PRIORITY_DEFAULT, notify_dnd_enter, notify, NULL);

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idropsourcenotify_dragleavetarget (IDropSourceNotify *This)
{
  source_drag_context *ctx = (source_drag_context *) (((char *) This) - G_STRUCT_OFFSET (source_drag_context, idsn));
  GdkWin32DnDEnterLeaveNotify *notify;

  if (!dnd_queue_is_empty ())
    process_dnd_queue (FALSE, 0, NULL);

  GDK_NOTE (DND, g_print ("idropsourcenotify_dragleavetarget %p (SDC %p) 0x%p\n", This, ctx, ctx->dest_window_handle));

  notify = g_new0 (GdkWin32DnDEnterLeaveNotify, 1);
  notify->target_window_handle = ctx->dest_window_handle;
  ctx->dest_window_handle = NULL;
  notify->opaque_context = ctx->context;
  g_idle_add_full (G_PRIORITY_DEFAULT, notify_dnd_leave, notify, NULL);

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idropsource_queryinterface (LPDROPSOURCE This,
                            REFIID       riid,
                            LPVOID      *ppvObject)
{
  GDK_NOTE (DND, {
      g_print ("idropsource_queryinterface %p ", This);
      PRINT_GUID (riid);
    });

  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      GDK_NOTE (DND, g_print ("...IUnknown S_OK\n"));
      idropsource_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDropSource))
    {
      GDK_NOTE (DND, g_print ("...IDropSource S_OK\n"));
      idropsource_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDropSourceNotify))
    {
      GDK_NOTE (DND, g_print ("...IDropSourceNotify S_OK\n"));
      idropsource_addref (This);
      *ppvObject = &((source_drag_context *) This)->idsn;
      return S_OK;
    }
  else
    {
      GDK_NOTE (DND, g_print ("...E_NOINTERFACE\n"));
      return E_NOINTERFACE;
    }
}

static gboolean
unref_context_in_main_thread (gpointer opaque_context)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (opaque_context);

  g_clear_object (&context);

  return G_SOURCE_REMOVE;
}

static ULONG STDMETHODCALLTYPE
idropsource_release (LPDROPSOURCE This)
{
  source_drag_context *ctx = (source_drag_context *) This;

  int ref_count = --ctx->ref_count;

  GDK_NOTE (DND, g_print ("idropsource_release %p %d\n", This, ref_count));

  if (ref_count == 0)
  {
    g_idle_add (unref_context_in_main_thread, ctx->context);
    g_free (This);
  }

  return ref_count;
}

/* NOTE: This method is called continuously, even if nothing is
 * happening, as long as the drag operation is in progress.
 * It is OK to return a "safe" value (S_OK, to keep the drag
 * operation going) even if something notable happens, because
 * we will have another opportunity to return the "right" value
 * (once we know what it is, after GTK processes the events we
 *  send out) very soon.
 * Note that keyboard-related state in this function is nonsense,
 * as DoDragDrop doesn't get precise information about the keyboard,
 * especially the fEscapePressed argument.
 */
static HRESULT STDMETHODCALLTYPE
idropsource_querycontinuedrag (LPDROPSOURCE This,
                               BOOL         fEscapePressed,
                               DWORD        grfKeyState)
{
  source_drag_context *ctx = (source_drag_context *) This;

  GDK_NOTE (DND, g_print ("idropsource_querycontinuedrag %p esc=%d keystate=0x%lx with state %d", This, fEscapePressed, grfKeyState, ctx->util_data.state));

  if (!dnd_queue_is_empty ())
    process_dnd_queue (FALSE, 0, NULL);

  if (ctx->util_data.state == GDK_WIN32_DND_DROPPED)
    {
      GDK_NOTE (DND, g_print ("DRAGDROP_S_DROP\n"));
      return DRAGDROP_S_DROP;
    }
  else if (ctx->util_data.state == GDK_WIN32_DND_NONE)
    {
      GDK_NOTE (DND, g_print ("DRAGDROP_S_CANCEL\n"));
      return DRAGDROP_S_CANCEL;
    }
  else
    {
      GDK_NOTE (DND, g_print ("S_OK\n"));
      return S_OK;
    }
}

static gboolean
give_feedback (gpointer user_data)
{
  GdkWin32DnDThreadGiveFeedback *feedback = (GdkWin32DnDThreadGiveFeedback *) user_data;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  gpointer ddd = g_hash_table_lookup (clipdrop->active_source_drags, feedback->base.opaque_context);

  if (ddd)
    {
      GdkDragContext *context = GDK_DRAG_CONTEXT (feedback->base.opaque_context);
      GdkWin32DragContext *win32_context = GDK_WIN32_DRAG_CONTEXT (context);

      GDK_NOTE (DND, g_print ("gdk_dnd_handle_drag_status: 0x%p\n",
                              context));

      context->action = action_for_drop_effect (feedback->received_drop_effect);

      if (context->action != win32_context->current_action)
        {
          win32_context->current_action = context->action;
          g_signal_emit_by_name (context, "action-changed", context->action);
        }
    }

  free_queue_item (&feedback->base);

  return G_SOURCE_REMOVE;
}

static HRESULT STDMETHODCALLTYPE
idropsource_givefeedback (LPDROPSOURCE This,
                          DWORD        dwEffect)
{
  source_drag_context *ctx = (source_drag_context *) This;
  GdkWin32DnDThreadGiveFeedback *feedback;

  GDK_NOTE (DND, g_print ("idropsource_givefeedback %p with drop effect %lu S_OK\n", This, dwEffect));

  if (!dnd_queue_is_empty ())
    process_dnd_queue (FALSE, 0, NULL);

  feedback = g_new0 (GdkWin32DnDThreadGiveFeedback, 1);
  feedback->base.opaque_context = ctx->context;
  feedback->received_drop_effect = dwEffect;

  g_idle_add_full (G_PRIORITY_DEFAULT, give_feedback, feedback, NULL);

  GDK_NOTE (DND, g_print ("idropsource_givefeedback %p returns\n", This));

  return S_OK;
}

static ULONG STDMETHODCALLTYPE
idataobject_addref (LPDATAOBJECT This)
{
  data_object *dobj = (data_object *) This;
  int ref_count = ++dobj->ref_count;

  GDK_NOTE (DND, g_print ("idataobject_addref %p %d\n", This, ref_count));

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
idataobject_queryinterface (LPDATAOBJECT This,
                            REFIID       riid,
                            LPVOID      *ppvObject)
{
  GDK_NOTE (DND, {
      g_print ("idataobject_queryinterface %p ", This);
      PRINT_GUID (riid);
    });

  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      GDK_NOTE (DND, g_print ("...IUnknown S_OK\n"));
      idataobject_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDataObject))
    {
      GDK_NOTE (DND, g_print ("...IDataObject S_OK\n"));
      idataobject_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else
    {
      GDK_NOTE (DND, g_print ("...E_NOINTERFACE\n"));
      return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE
idataobject_release (LPDATAOBJECT This)
{
  data_object *dobj = (data_object *) This;
  int ref_count = --dobj->ref_count;

  GDK_NOTE (DND, g_print ("idataobject_release %p %d\n", This, ref_count));

  if (ref_count == 0)
    {
      g_array_free (dobj->formats, TRUE);
      g_free (This);
    }

  return ref_count;
}

static HRESULT
query (LPDATAOBJECT                This,
       LPFORMATETC                 pFormatEtc,
       GdkWin32ContentFormatPair **pair)
{
  data_object *ctx = (data_object *) This;
  gint i;

  if (pair)
    *pair = NULL;

  if (!pFormatEtc)
    return DV_E_FORMATETC;

  if (pFormatEtc->lindex != -1)
    return DV_E_LINDEX;

  if ((pFormatEtc->tymed & TYMED_HGLOBAL) == 0)
    return DV_E_TYMED;

  if ((pFormatEtc->dwAspect & DVASPECT_CONTENT) == 0)
    return DV_E_DVASPECT;

  for (i = 0; i < ctx->formats->len; i++)
    {
      GdkWin32ContentFormatPair *p = &g_array_index (ctx->formats, GdkWin32ContentFormatPair, i);
      if (pFormatEtc->cfFormat == p->w32format)
        {
          if (pair)
            *pair = p;

          return S_OK;
        }
    }

  return DV_E_FORMATETC;
}

static HRESULT STDMETHODCALLTYPE
idataobject_getdata (LPDATAOBJECT This,
                     LPFORMATETC  pFormatEtc,
                     LPSTGMEDIUM  pMedium)
{
  data_object *ctx = (data_object *) This;
  HRESULT hr;
  GdkWin32DnDThreadGetData *getdata;
  GdkWin32ContentFormatPair *pair;

  if (ctx->context == NULL)
    return E_FAIL;

  GDK_NOTE (DND, g_print ("idataobject_getdata %p %s ",
                          This, _gdk_win32_cf_to_string (pFormatEtc->cfFormat)));

  /* Check whether we can provide requested format */
  hr = query (This, pFormatEtc, &pair);

  if (hr != S_OK)
    {
      GDK_NOTE (DND, g_print ("Unsupported format, returning 0x%lx\n", hr));
      return hr;
    }

  if (!dnd_queue_is_empty ())
    process_dnd_queue (FALSE, 0, NULL);

  getdata = g_new0 (GdkWin32DnDThreadGetData, 1);
  getdata->base.item_type = GDK_WIN32_DND_THREAD_QUEUE_ITEM_GET_DATA;
  getdata->base.opaque_context = (gpointer) ctx->context;
  getdata->pair = *pair;
  g_idle_add_full (G_PRIORITY_DEFAULT, get_data_response, getdata, NULL);

  if (!process_dnd_queue (TRUE, g_get_monotonic_time () + G_USEC_PER_SEC * 30, getdata))
    return E_FAIL;

  if (getdata->produced_data_medium.tymed == TYMED_NULL)
    {
      free_queue_item (&getdata->base);

      return E_FAIL;
    }

  memcpy (pMedium, &getdata->produced_data_medium, sizeof (*pMedium));

  /* To ensure that the data isn't freed */
  getdata->produced_data_medium.tymed = TYMED_NULL;

  free_queue_item (&getdata->base);

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idataobject_getdatahere (LPDATAOBJECT This,
                         LPFORMATETC  pFormatEtc,
                         LPSTGMEDIUM  pMedium)
{
  GDK_NOTE (DND, g_print ("idataobject_getdatahere %p E_NOTIMPL\n", This));

  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_querygetdata (LPDATAOBJECT This,
                          LPFORMATETC  pFormatEtc)
{
  HRESULT hr;

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread != g_thread_self ());

  hr = query (This, pFormatEtc, NULL);

  GDK_NOTE (DND,
      g_print ("idataobject_querygetdata %p 0x%08x fmt, %p ptd, %lu aspect, %ld lindex, %0lx tymed - %s, return %#lx (%s)\n",
               This, pFormatEtc->cfFormat, pFormatEtc->ptd, pFormatEtc->dwAspect, pFormatEtc->lindex, pFormatEtc->tymed, _gdk_win32_cf_to_string (pFormatEtc->cfFormat),
               hr, (hr == S_OK) ? "S_OK" : (hr == DV_E_FORMATETC) ? "DV_E_FORMATETC" : (hr == DV_E_LINDEX) ? "DV_E_LINDEX" : (hr == DV_E_TYMED) ? "DV_E_TYMED" : (hr == DV_E_DVASPECT) ? "DV_E_DVASPECT" : "uknown meaning"));

  return hr;
}

static HRESULT STDMETHODCALLTYPE
idataobject_getcanonicalformatetc (LPDATAOBJECT This,
                                   LPFORMATETC  pFormatEtcIn,
                                   LPFORMATETC  pFormatEtcOut)
{
  GDK_NOTE (DND, g_print ("idataobject_getcanonicalformatetc %p E_NOTIMPL\n", This));

  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_setdata (LPDATAOBJECT This,
                     LPFORMATETC  pFormatEtc,
                     LPSTGMEDIUM  pMedium,
                     BOOL         fRelease)
{
  GDK_NOTE (DND, g_print ("idataobject_setdata %p %s E_NOTIMPL\n",
                          This, _gdk_win32_cf_to_string (pFormatEtc->cfFormat)));

  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_enumformatetc (LPDATAOBJECT     This,
                           DWORD            dwDirection,
                           LPENUMFORMATETC *ppEnumFormatEtc)
{
  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread != g_thread_self ());

  if (dwDirection != DATADIR_GET)
    {
      GDK_NOTE (DND, g_print ("idataobject_enumformatetc %p E_NOTIMPL", This));

      return E_NOTIMPL;
    }

  *ppEnumFormatEtc = &enum_formats_new (((data_object *) This)->formats)->ief;

  GDK_NOTE (DND, g_print ("idataobject_enumformatetc %p -> %p S_OK", This, *ppEnumFormatEtc));

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idataobject_dadvise (LPDATAOBJECT This,
                     LPFORMATETC  pFormatetc,
                     DWORD        advf,
                     LPADVISESINK pAdvSink,
                     DWORD       *pdwConnection)
{
  GDK_NOTE (DND, g_print ("idataobject_dadvise %p E_NOTIMPL\n", This));

  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_dunadvise (LPDATAOBJECT This,
                       DWORD         dwConnection)
{
  GDK_NOTE (DND, g_print ("idataobject_dunadvise %p E_NOTIMPL\n", This));

  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_enumdadvise (LPDATAOBJECT    This,
                         LPENUMSTATDATA *ppenumAdvise)
{
  GDK_NOTE (DND, g_print ("idataobject_enumdadvise %p OLE_E_ADVISENOTSUPPORTED\n", This));

  return OLE_E_ADVISENOTSUPPORTED;
}

static ULONG STDMETHODCALLTYPE
ienumformatetc_addref (LPENUMFORMATETC This)
{
  enum_formats *en = (enum_formats *) This;
  int ref_count = ++en->ref_count;

  GDK_NOTE (DND, g_print ("ienumformatetc_addref %p %d\n", This, ref_count));

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_queryinterface (LPENUMFORMATETC This,
                               REFIID          riid,
                               LPVOID         *ppvObject)
{
  GDK_NOTE (DND, {
      g_print ("ienumformatetc_queryinterface %p", This);
      PRINT_GUID (riid);
    });

  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      GDK_NOTE (DND, g_print ("...IUnknown S_OK\n"));
      ienumformatetc_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IEnumFORMATETC))
    {
      GDK_NOTE (DND, g_print ("...IEnumFORMATETC S_OK\n"));
      ienumformatetc_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else
    {
      GDK_NOTE (DND, g_print ("...E_NOINTERFACE\n"));
      return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE
ienumformatetc_release (LPENUMFORMATETC This)
{
  enum_formats *en = (enum_formats *) This;
  int ref_count = --en->ref_count;

  GDK_NOTE (DND, g_print ("ienumformatetc_release %p %d\n", This, ref_count));

  if (ref_count == 0)
    {
      g_array_unref (en->formats);
      g_free (This);
    }

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_next (LPENUMFORMATETC This,
                     ULONG             celt,
                     LPFORMATETC     elts,
                     ULONG            *nelt)
{
  enum_formats *en = (enum_formats *) This;
  ULONG i, n;
  ULONG formats_to_get = celt;

  GDK_NOTE (DND, g_print ("ienumformatetc_next %p %d %ld ", This, en->ix, celt));

  n = 0;
  for (i = 0; i < formats_to_get; i++)
    {
      UINT fmt;
      if (en->ix >= en->formats->len)
        break;
      fmt = g_array_index (en->formats, GdkWin32ContentFormatPair, en->ix++).w32format;
      /* skip internals */
      if (fmt == 0 || fmt > 0xFFFF)
        {
          formats_to_get += 1;
          continue;
        }
      elts[n].cfFormat = fmt;
      elts[n].ptd = NULL;
      elts[n].dwAspect = DVASPECT_CONTENT;
      elts[n].lindex = -1;
      elts[n].tymed = TYMED_HGLOBAL;

      n++;
    }

  if (nelt != NULL)
    *nelt = n;

  GDK_NOTE (DND, g_print ("%s\n", (n == celt) ? "S_OK" : "S_FALSE"));

  if (n == celt)
    return S_OK;
  else
    return S_FALSE;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_skip (LPENUMFORMATETC This,
                     ULONG             celt)
{
  enum_formats *en = (enum_formats *) This;

  GDK_NOTE (DND, g_print ("ienumformatetc_skip %p %d %ld S_OK\n", This, en->ix, celt));

  en->ix += celt;

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_reset (LPENUMFORMATETC This)
{
  enum_formats *en = (enum_formats *) This;

  GDK_NOTE (DND, g_print ("ienumformatetc_reset %p S_OK\n", This));

  en->ix = 0;

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_clone (LPENUMFORMATETC  This,
                      LPENUMFORMATETC *ppEnumFormatEtc)
{
  enum_formats *en = (enum_formats *) This;
  enum_formats *new;

  GDK_NOTE (DND, g_print ("ienumformatetc_clone %p S_OK\n", This));

  new = enum_formats_new (en->formats);

  new->ix = en->ix;

  *ppEnumFormatEtc = &new->ief;

  return S_OK;
}

static IDropSourceVtbl ids_vtbl = {
  idropsource_queryinterface,
  idropsource_addref,
  idropsource_release,
  idropsource_querycontinuedrag,
  idropsource_givefeedback
};

static IDropSourceNotifyVtbl idsn_vtbl = {
  (HRESULT (STDMETHODCALLTYPE *) (IDropSourceNotify *, REFIID , LPVOID *)) idropsource_queryinterface,
  (ULONG (STDMETHODCALLTYPE *) (IDropSourceNotify *)) idropsource_addref,
  (ULONG (STDMETHODCALLTYPE *) (IDropSourceNotify *)) idropsource_release,
  idropsourcenotify_dragentertarget,
  idropsourcenotify_dragleavetarget
};

static IDataObjectVtbl ido_vtbl = {
  idataobject_queryinterface,
  idataobject_addref,
  idataobject_release,
  idataobject_getdata,
  idataobject_getdatahere,
  idataobject_querygetdata,
  idataobject_getcanonicalformatetc,
  idataobject_setdata,
  idataobject_enumformatetc,
  idataobject_dadvise,
  idataobject_dunadvise,
  idataobject_enumdadvise
};

static IEnumFORMATETCVtbl ief_vtbl = {
  ienumformatetc_queryinterface,
  ienumformatetc_addref,
  ienumformatetc_release,
  ienumformatetc_next,
  ienumformatetc_skip,
  ienumformatetc_reset,
  ienumformatetc_clone
};

static source_drag_context *
source_context_new (GdkDragContext    *context,
                    GdkSurface        *window,
                    GdkContentFormats *formats)
{
  GdkWin32DragContext *context_win32;
  source_drag_context *result;

  context_win32 = GDK_WIN32_DRAG_CONTEXT (context);

  result = g_new0 (source_drag_context, 1);
  result->context = g_object_ref (context);
  result->ids.lpVtbl = &ids_vtbl;
  result->idsn.lpVtbl = &idsn_vtbl;
  result->ref_count = 1;
  result->source_window_handle = GDK_SURFACE_HWND (context->source_surface);
  result->scale = context_win32->scale;
  result->util_data.state = GDK_WIN32_DND_PENDING; /* Implicit */

  GDK_NOTE (DND, g_print ("source_context_new: %p (drag context %p)\n", result, result->context));

  return result;
}

static data_object *
data_object_new (GdkDragContext *context)
{
  data_object *result;
  const char * const *mime_types;
  gsize n_mime_types, i;

  result = g_new0 (data_object, 1);

  result->ido.lpVtbl = &ido_vtbl;
  result->ref_count = 1;
  result->context = context;
  result->formats = g_array_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair));

  mime_types = gdk_content_formats_get_mime_types (gdk_drag_context_get_formats (context), &n_mime_types);

  for (i = 0; i < n_mime_types; i++)
    {
      gint added_count = 0;
      gint j;

      GDK_NOTE (DND, g_print ("DataObject supports contentformat 0x%p (%s)\n", mime_types[i], mime_types[i]));

      added_count = _gdk_win32_add_contentformat_to_pairs (mime_types[i], result->formats);

      for (j = 0; j < added_count && result->formats->len - 1 - j >= 0; j++)
        GDK_NOTE (DND, g_print ("DataObject will support w32format 0x%x\n", g_array_index (result->formats, GdkWin32ContentFormatPair, j).w32format));
    }

  GDK_NOTE (DND, g_print ("data_object_new: %p\n", result));

  return result;
}

static enum_formats *
enum_formats_new (GArray *formats)
{
  enum_formats *result;

  result = g_new0 (enum_formats, 1);

  result->ief.lpVtbl = &ief_vtbl;
  result->ref_count = 1;
  result->ix = 0;
  result->formats = g_array_ref (formats);

  return result;
}

void
_gdk_drag_init (void)
{
  CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);

  if (g_strcmp0 (getenv ("GDK_WIN32_OLE2_DND"), "0") != 0)
    use_ole2_dnd = TRUE;

  if (use_ole2_dnd)
    {
      HRESULT hr;

      hr = OleInitialize (NULL);

      if (! SUCCEEDED (hr))
        g_error ("OleInitialize failed");
    }
}

void
_gdk_win32_dnd_exit (void)
{
  if (use_ole2_dnd)
    {
      OleUninitialize ();
    }

  CoUninitialize ();
}

/* Source side */

void
_gdk_win32_drag_context_send_local_status_event (GdkDragContext *src_context,
                                                 GdkDragAction   action)
{
  GdkWin32DragContext *src_context_win32 = GDK_WIN32_DRAG_CONTEXT (src_context);

  if (src_context_win32->drag_status == GDK_DRAG_STATUS_MOTION_WAIT)
    src_context_win32->drag_status = GDK_DRAG_STATUS_DRAG;

  src_context->action = action;

  GDK_NOTE (DND, g_print ("gdk_dnd_handle_drag_status: 0x%p\n",
                          src_context));

  if (action != src_context_win32->current_action)
    {
      src_context_win32->current_action = action;
      g_signal_emit_by_name (src_context, "action-changed", action);
    }
}

static void
local_send_leave (GdkDragContext *context,
                  guint32         time)
{
  GdkEvent *tmp_event;

  GDK_NOTE (DND, g_print ("local_send_leave: context=%p current_dest_drag=%p\n",
                          context,
                          current_dest_drag));

  if ((current_dest_drag != NULL) &&
      (GDK_WIN32_DRAG_CONTEXT (current_dest_drag)->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_surface == context->source_surface))
    {
      gdk_drop_emit_leave_event (GDK_DROP (current_dest_drag), FALSE, GDK_CURRENT_TIME);

      current_dest_drag = NULL;
    }
}

static void
local_send_motion (GdkDragContext *context,
                   gint            x_root,
                   gint            y_root,
                   GdkDragAction   action,
                   guint32         time)
{
  GdkWin32DragContext *context_win32 = GDK_WIN32_DRAG_CONTEXT (context);

  GDK_NOTE (DND, g_print ("local_send_motion: context=%p (%d,%d) current_dest_drag=%p\n",
                          context, x_root, y_root,
                          current_dest_drag));

  if ((current_dest_drag != NULL) &&
      (GDK_WIN32_DRAG_CONTEXT (current_dest_drag)->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_surface == context->source_surface))
    {
      GdkWin32DragContext *current_dest_drag_win32;

      gdk_drag_context_set_actions (current_dest_drag, action, action);

      current_dest_drag_win32 = GDK_WIN32_DRAG_CONTEXT (current_dest_drag);
      current_dest_drag_win32->util_data.last_x = x_root;
      current_dest_drag_win32->util_data.last_y = y_root;

      context_win32->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;

      gdk_drop_emit_motion_event (GDK_DROP (current_dest_drag), FALSE, x_root, y_root, time);
    }
}

static void
local_send_drop (GdkDragContext *context,
                 guint32         time)
{
  GDK_NOTE (DND, g_print ("local_send_drop: context=%p current_dest_drag=%p\n",
                          context,
                          current_dest_drag));

  if ((current_dest_drag != NULL) &&
      (GDK_WIN32_DRAG_CONTEXT (current_dest_drag)->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_surface == context->source_surface))
    {
      GdkWin32DragContext *context_win32;

      context_win32 = GDK_WIN32_DRAG_CONTEXT (current_dest_drag);

      gdk_drop_emit_motion_event (GDK_DROP (current_dest_drag),
                                  FALSE,
                                  context_win32->util_data.last_x, context_win32->util_data.last_y,
                                  GDK_CURRENT_TIME);

      current_dest_drag = NULL;
    }
}

void
_gdk_win32_drag_do_leave (GdkDragContext *context,
                          guint32         time)
{
  if (context->dest_surface)
    {
      GDK_NOTE (DND, g_print ("gdk_drag_do_leave\n"));

      if (!use_ole2_dnd)
        {
          if (GDK_WIN32_DRAG_CONTEXT (context)->protocol == GDK_DRAG_PROTO_LOCAL)
            local_send_leave (context, time);
        }

      g_clear_object (&context->dest_surface);
    }
}

static GdkSurface *
create_drag_surface (GdkDisplay *display)
{
  GdkSurface *window;

  window = gdk_surface_new_popup (display, &(GdkRectangle) { 0, 0, 100, 100 });

  gdk_surface_set_type_hint (window, GDK_SURFACE_TYPE_HINT_DND);

  return window;
}

GdkDragContext *
_gdk_win32_surface_drag_begin (GdkSurface        *window,
                              GdkDevice          *device,
                              GdkContentProvider *content,
                              GdkDragAction       actions,
                              gint                dx,
                              gint                dy)
{
  GdkDragContext *context;
  GdkWin32DragContext *context_win32;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  int x_root, y_root;

  g_return_val_if_fail (window != NULL, NULL);

  context = gdk_drag_context_new (gdk_surface_get_display (window),
                                  content,
                                  window,
                                  gdk_content_formats_union_serialize_mime_types (gdk_content_provider_ref_storable_formats (content)),
                                  actions,
                                  device,
                                  use_ole2_dnd ? GDK_DRAG_PROTO_OLE2 : GDK_DRAG_PROTO_LOCAL);

  context_win32 = GDK_WIN32_DRAG_CONTEXT (context);

  GDK_NOTE (DND, g_print ("gdk_drag_begin\n"));

  gdk_device_get_position (device, &x_root, &y_root);
  x_root += dx;
  y_root += dy;

  context_win32->start_x = x_root;
  context_win32->start_y = y_root;
  context_win32->util_data.last_x = context_win32->start_x;
  context_win32->util_data.last_y = context_win32->start_y;

  g_set_object (&context_win32->ipc_window, window);

  context_win32->drag_surface = create_drag_surface (gdk_surface_get_display (window));

  if (!drag_context_grab (context))
    {
      g_object_unref (context);
      return FALSE;
    }

  if (use_ole2_dnd)
    {
      GdkWin32DnDThreadDoDragDrop *ddd = g_new0 (GdkWin32DnDThreadDoDragDrop, 1);
      source_drag_context *source_ctx;
      data_object         *data_obj;

      source_ctx = source_context_new (context, 
                                       window,
                                       gdk_drag_context_get_formats (context));
      data_obj = data_object_new (context);

      ddd->base.item_type = GDK_WIN32_DND_THREAD_QUEUE_ITEM_DO_DRAG_DROP;
      ddd->base.opaque_context = context_win32;
      ddd->src_context = source_ctx;
      ddd->src_object = data_obj;
      ddd->allowed_drop_effects = 0;
      if (actions & GDK_ACTION_COPY)
        ddd->allowed_drop_effects |= DROPEFFECT_COPY;
      if (actions & GDK_ACTION_MOVE)
        ddd->allowed_drop_effects |= DROPEFFECT_MOVE;
      if (actions & GDK_ACTION_LINK)
        ddd->allowed_drop_effects |= DROPEFFECT_LINK;

      g_hash_table_replace (clipdrop->active_source_drags, g_object_ref (context), ddd);
      increment_dnd_queue_counter ();
      g_async_queue_push (clipdrop->dnd_queue, ddd);
      API_CALL (PostThreadMessage, (clipdrop->dnd_thread_id, thread_wakeup_message, 0, 0));

      context_win32->util_data.state = GDK_WIN32_DND_PENDING;
    }

  move_drag_surface (context, x_root, y_root);

  return context;
}

/* TODO: remove this?
 * window finder is only used by our gdk_drag_update() to
 * find the window at drag coordinates - which is
 * something IDropSourceNotify already gives us.
 * Unless, of course, we keep the LOCAL protocol around.
 */
typedef struct {
  gint x;
  gint y;
  HWND ignore;
  HWND result;
} find_window_enum_arg;

static BOOL CALLBACK
find_window_enum_proc (HWND   hwnd,
                       LPARAM lparam)
{
  RECT rect;
  POINT tl, br;
  find_window_enum_arg *a = (find_window_enum_arg *) lparam;

  if (hwnd == a->ignore)
    return TRUE;

  if (!IsWindowVisible (hwnd))
    return TRUE;

  tl.x = tl.y = 0;
  ClientToScreen (hwnd, &tl);
  GetClientRect (hwnd, &rect);
  br.x = rect.right;
  br.y = rect.bottom;
  ClientToScreen (hwnd, &br);

  if (a->x >= tl.x && a->y >= tl.y && a->x < br.x && a->y < br.y)
    {
      a->result = hwnd;
      return FALSE;
    }
  else
    return TRUE;
}

static GdkSurface *
gdk_win32_drag_context_find_surface (GdkDragContext  *context,
                                     GdkSurface      *drag_surface,
                                     gint             x_root,
                                     gint             y_root,
                                     GdkDragProtocol *protocol)
{
  GdkWin32DragContext *context_win32 = GDK_WIN32_DRAG_CONTEXT (context);
  GdkSurface *dest_surface, *dw;
  find_window_enum_arg a;

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  a.x = x_root * context_win32->scale - _gdk_offset_x;
  a.y = y_root * context_win32->scale - _gdk_offset_y;
  a.ignore = drag_surface ? GDK_SURFACE_HWND (drag_surface) : NULL;
  a.result = NULL;

  GDK_NOTE (DND,
            g_print ("gdk_win32_drag_context_find_surface: %p %+d%+d\n",
                     (drag_surface ? GDK_SURFACE_HWND (drag_surface) : NULL),
                     a.x, a.y));

  EnumWindows (find_window_enum_proc, (LPARAM) &a);

  if (a.result == NULL)
    dest_surface = NULL;
  else
    {
      dw = gdk_win32_handle_table_lookup (a.result);
      if (dw)
        {
          dest_surface = gdk_surface_get_toplevel (dw);
          g_object_ref (dest_surface);
        }
      else
        dest_surface = gdk_win32_surface_foreign_new_for_display (gdk_drag_context_get_display (context), a.result);

      if (use_ole2_dnd)
        *protocol = GDK_DRAG_PROTO_OLE2;
      else if (context->source_surface)
        *protocol = GDK_DRAG_PROTO_LOCAL;
      else
        *protocol = GDK_DRAG_PROTO_WIN32_DROPFILES;
    }

  GDK_NOTE (DND,
            g_print ("gdk_win32_drag_context_find_surface: %p %+d%+d: %p: %p %s\n",
                     (drag_surface ? GDK_SURFACE_HWND (drag_surface) : NULL),
                     x_root, y_root,
                     a.result,
                     (dest_surface ? GDK_SURFACE_HWND (dest_surface) : NULL),
                     _gdk_win32_drag_protocol_to_string (*protocol)));

  return dest_surface;
}

static gboolean
gdk_win32_drag_context_drag_motion (GdkDragContext  *context,
                                    GdkSurface      *dest_surface,
                                    GdkDragProtocol  protocol,
                                    gint             x_root,
                                    gint             y_root,
                                    GdkDragAction    suggested_action,
                                    GdkDragAction    possible_actions,
                                    guint32          time)
{
  GdkWin32DragContext *context_win32;

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  g_return_val_if_fail (context != NULL, FALSE);

  GDK_NOTE (DND, g_print ("gdk_win32_drag_context_drag_motion: @ %+d:%+d %s suggested=%s, possible=%s\n"
                          " context=%p:{actions=%s,suggested=%s,action=%s}\n",
                          x_root, y_root,
                          _gdk_win32_drag_protocol_to_string (protocol),
                          _gdk_win32_drag_action_to_string (suggested_action),
                          _gdk_win32_drag_action_to_string (possible_actions),
                          context,
                          _gdk_win32_drag_action_to_string (gdk_drag_context_get_actions (context)),
                          _gdk_win32_drag_action_to_string (gdk_drag_context_get_suggested_action (context)),
                          _gdk_win32_drag_action_to_string (context->action)));

  context_win32 = GDK_WIN32_DRAG_CONTEXT (context);

  if (context_win32->drag_surface)
    move_drag_surface (context, x_root, y_root);

  if (!use_ole2_dnd)
    {
      if (context->dest_surface == dest_surface)
        {
          GdkDragContext *dest_context;

          dest_context = _gdk_win32_drop_context_find (context->source_surface,
                                                       dest_surface);

          if (dest_context)
            gdk_drag_context_set_actions (dest_context, possible_actions, suggested_action);

          gdk_drag_context_set_actions (context, possible_actions, suggested_action);
        }
      else
        {
          /* Send a leave to the last destination */
          _gdk_win32_drag_do_leave (context, time);

          context_win32->drag_status = GDK_DRAG_STATUS_DRAG;

          /* Check if new destination accepts drags, and which protocol */
          if (dest_surface)
            {
              g_set_object (&context->dest_surface, dest_surface);
              context_win32->protocol = protocol;

              switch (protocol)
                {
                case GDK_DRAG_PROTO_LOCAL:
                  _gdk_win32_local_send_enter (context, time);
                  break;

                default:
                  break;
                }
              gdk_drag_context_set_actions (context, possible_actions, suggested_action);
            }
          else
            {
              context->dest_surface = NULL;
              gdk_drag_context_set_actions (context, 0, 0);
            }

          GDK_NOTE (DND, g_print ("gdk_dnd_handle_drag_status: 0x%p\n",
                                  context));

          if (context->action != context_win32->current_action)
            {
              context_win32->current_action = context->action;
              g_signal_emit_by_name (context, "action-changed", context->action);
            }
        }

      /* Send a drag-motion event */

      context_win32->util_data.last_x = x_root;
      context_win32->util_data.last_y = y_root;

      if (context->dest_surface)
        {
          if (context_win32->drag_status == GDK_DRAG_STATUS_DRAG)
            {
              switch (context_win32->protocol)
                {
                case GDK_DRAG_PROTO_LOCAL:
                  local_send_motion (context, x_root, y_root, suggested_action, time);
                  break;

                case GDK_DRAG_PROTO_NONE:
                  g_warning ("GDK_DRAG_PROTO_NONE is not valid in gdk_win32_drag_context_drag_motion()");
                  break;

                default:
                  break;
                }
            }
          else
            {
              GDK_NOTE (DND, g_print (" returning TRUE\n"
                                      " context=%p:{actions=%s,suggested=%s,action=%s}\n",
                                      context,
                                      _gdk_win32_drag_action_to_string (gdk_drag_context_get_actions (context)),
                                      _gdk_win32_drag_action_to_string (gdk_drag_context_get_suggested_action (context)),
                                      _gdk_win32_drag_action_to_string (context->action)));
              return TRUE;
            }
        }
    }

  GDK_NOTE (DND, g_print (" returning FALSE\n"
                          " context=%p:{actions=%s,suggested=%s,action=%s}\n",
                          context,
                          _gdk_win32_drag_action_to_string (gdk_drag_context_get_actions (context)),
                          _gdk_win32_drag_action_to_string (gdk_drag_context_get_suggested_action (context)),
                          _gdk_win32_drag_action_to_string (context->action)));
  return FALSE;
}

static void
send_source_state_update (GdkWin32Clipdrop    *clipdrop,
                          GdkWin32DragContext *context_win32,
                          gpointer            *ddd)
{
  GdkWin32DnDThreadUpdateDragState *status = g_new0 (GdkWin32DnDThreadUpdateDragState, 1);
  status->base.item_type = GDK_WIN32_DND_THREAD_QUEUE_ITEM_UPDATE_DRAG_STATE;
  status->opaque_ddd = ddd;
  status->produced_util_data = context_win32->util_data;
  increment_dnd_queue_counter ();
  g_async_queue_push (clipdrop->dnd_queue, status);
  API_CALL (PostThreadMessage, (clipdrop->dnd_thread_id, thread_wakeup_message, 0, 0));
}

static void
gdk_win32_drag_context_drag_drop (GdkDragContext *context,
                                  guint32         time)
{
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drag_drop\n"));

  if (!use_ole2_dnd)
    {
      if (context->dest_surface &&
          GDK_WIN32_DRAG_CONTEXT (context)->protocol == GDK_DRAG_PROTO_LOCAL)
        local_send_drop (context, time);
    }
  else
    {
      gpointer ddd = g_hash_table_lookup (clipdrop->active_source_drags, context);

      GDK_WIN32_DRAG_CONTEXT (context)->util_data.state = GDK_WIN32_DND_DROPPED;

      if (ddd)
        send_source_state_update (clipdrop, GDK_WIN32_DRAG_CONTEXT (context), ddd);
    }
}

static void
gdk_win32_drag_context_drag_abort (GdkDragContext *context,
                                   guint32         time)
{
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drag_abort\n"));

  if (use_ole2_dnd)
    {
      gpointer ddd = g_hash_table_lookup (clipdrop->active_source_drags, context);

      GDK_WIN32_DRAG_CONTEXT (context)->util_data.state = GDK_WIN32_DND_NONE;

      if (ddd)
        send_source_state_update (clipdrop, GDK_WIN32_DRAG_CONTEXT (context), ddd);
    }
}

static void
gdk_win32_drag_context_set_cursor (GdkDragContext *context,
                                   GdkCursor      *cursor)
{
  GdkWin32DragContext *context_win32 = GDK_WIN32_DRAG_CONTEXT (context);

  GDK_NOTE (DND, g_print ("gdk_drag_context_set_cursor: 0x%p 0x%p\n", context, cursor));

  if (!g_set_object (&context_win32->cursor, cursor))
    return;

  if (context_win32->grab_seat)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gdk_device_grab (gdk_seat_get_pointer (context_win32->grab_seat),
                       context_win32->ipc_window,
                       GDK_OWNERSHIP_APPLICATION, FALSE,
                       GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                       cursor, GDK_CURRENT_TIME);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
}

static double
ease_out_cubic (double t)
{
  double p = t - 1;
  return p * p * p + 1;
}

#define ANIM_TIME 500000 /* half a second */

typedef struct _GdkDragAnim GdkDragAnim;
struct _GdkDragAnim {
  GdkWin32DragContext *context;
  GdkFrameClock *frame_clock;
  gint64 start_time;
};

static void
gdk_drag_anim_destroy (GdkDragAnim *anim)
{
  g_object_unref (anim->context);
  g_slice_free (GdkDragAnim, anim);
}

static gboolean
gdk_drag_anim_timeout (gpointer data)
{
  GdkDragAnim *anim = data;
  GdkWin32DragContext *context = anim->context;
  GdkFrameClock *frame_clock = anim->frame_clock;
  gint64 current_time;
  double f;
  double t;

  if (!frame_clock)
    return G_SOURCE_REMOVE;

  current_time = gdk_frame_clock_get_frame_time (frame_clock);

  f = (current_time - anim->start_time) / (double) ANIM_TIME;

  if (f >= 1.0)
    return G_SOURCE_REMOVE;

  t = ease_out_cubic (f);

  gdk_surface_show (context->drag_surface);
  gdk_surface_move (context->drag_surface,
                    context->util_data.last_x + (context->start_x - context->util_data.last_x) * t - context->hot_x,
                    context->util_data.last_y + (context->start_y - context->util_data.last_y) * t - context->hot_y);
  gdk_surface_set_opacity (context->drag_surface, 1.0 - f);

  return G_SOURCE_CONTINUE;
}

static void
gdk_win32_drag_context_drop_done (GdkDragContext *context,
                                  gboolean        success)
{
  GdkWin32DragContext *win32_context = GDK_WIN32_DRAG_CONTEXT (context);
  GdkDragAnim *anim;
/*
  cairo_surface_t *win_surface;
  cairo_surface_t *surface;
  cairo_t *cr;
*/
  guint id;

  GDK_NOTE (DND, g_print ("gdk_drag_context_drop_done: 0x%p %s\n",
                          context,
                          success ? "dropped successfully" : "dropped unsuccessfully"));

  /* FIXME: This is temporary, until the code is fixed to ensure that
   * gdk_drag_finish () is called by GTK.
   */
  if (use_ole2_dnd)
    {
      GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
      gpointer ddd = g_hash_table_lookup (clipdrop->active_source_drags, context);

      if (success)
        win32_context->util_data.state = GDK_WIN32_DND_DROPPED;
      else
        win32_context->util_data.state = GDK_WIN32_DND_NONE;

      if (ddd)
        send_source_state_update (clipdrop, win32_context, ddd);
    }

  if (success)
    {
      gdk_surface_hide (win32_context->drag_surface);

      return;
    }

/*
  win_surface = _gdk_surface_ref_cairo_surface (win32_context->drag_surface);
  surface = gdk_surface_create_similar_surface (win32_context->drag_surface,
                                                cairo_surface_get_content (win_surface),
                                                gdk_surface_get_width (win32_context->drag_surface),
                                                gdk_surface_get_height (win32_context->drag_surface));
  cr = cairo_create (surface);
  cairo_set_source_surface (cr, win_surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
  cairo_surface_destroy (win_surface);

  pattern = cairo_pattern_create_for_surface (surface);

  gdk_surface_set_background_pattern (win32_context->drag_surface, pattern);

  cairo_pattern_destroy (pattern);
  cairo_surface_destroy (surface);
*/

  anim = g_slice_new0 (GdkDragAnim);
  g_set_object (&anim->context, win32_context);
  anim->frame_clock = gdk_surface_get_frame_clock (win32_context->drag_surface);
  anim->start_time = gdk_frame_clock_get_frame_time (anim->frame_clock);

  GDK_NOTE (DND, g_print ("gdk_drag_context_drop_done: animate the drag window from %d : %d to %d : %d\n",
                          win32_context->util_data.last_x, win32_context->util_data.last_y,
                          win32_context->start_x, win32_context->start_y));

  id = g_timeout_add_full (G_PRIORITY_DEFAULT, 17,
                           gdk_drag_anim_timeout, anim,
                           (GDestroyNotify) gdk_drag_anim_destroy);
  g_source_set_name_by_id (id, "[gtk+] gdk_drag_anim_timeout");
}

static gboolean
drag_context_grab (GdkDragContext *context)
{
  GdkWin32DragContext *context_win32 = GDK_WIN32_DRAG_CONTEXT (context);
  GdkSeatCapabilities capabilities;
  GdkSeat *seat;
  GdkCursor *cursor;

  if (!context_win32->ipc_window)
    return FALSE;

  seat = gdk_device_get_seat (gdk_drag_context_get_device (context));

  capabilities = GDK_SEAT_CAPABILITY_ALL;

  cursor = gdk_drag_get_cursor (context, gdk_drag_context_get_selected_action (context));
  g_set_object (&context_win32->cursor, cursor);

  if (gdk_seat_grab (seat, context_win32->ipc_window,
                     capabilities, FALSE,
                     context_win32->cursor, NULL, NULL, NULL) != GDK_GRAB_SUCCESS)
    return FALSE;

  g_set_object (&context_win32->grab_seat, seat);

  /* TODO: Should be grabbing keys here, to support keynav. SetWindowsHookEx()? */

  return TRUE;
}

static void
drag_context_ungrab (GdkDragContext *context)
{
  GdkWin32DragContext *context_win32 = GDK_WIN32_DRAG_CONTEXT (context);

  if (!context_win32->grab_seat)
    return;

  gdk_seat_ungrab (context_win32->grab_seat);

  g_clear_object (&context_win32->grab_seat);

  /* TODO: Should be ungrabbing keys here */
}

static void
gdk_win32_drag_context_cancel (GdkDragContext      *context,
                               GdkDragCancelReason  reason)
{
  const gchar *reason_str = NULL;
  switch (reason)
    {
    case GDK_DRAG_CANCEL_NO_TARGET:
      reason_str = "no target";
      break;
    case GDK_DRAG_CANCEL_USER_CANCELLED:
      reason_str = "user cancelled";
      break;
    case GDK_DRAG_CANCEL_ERROR:
      reason_str = "error";
      break;
    default:
      reason_str = "<unknown>";
      break;
    }

  GDK_NOTE (DND, g_print ("gdk_drag_context_cancel: 0x%p %s\n",
                          context,
                          reason_str));
  drag_context_ungrab (context);
  gdk_drag_drop_done (context, FALSE);
}

static void
gdk_win32_drag_context_drop_performed (GdkDragContext *context,
                                       guint32         time_)
{
  GDK_NOTE (DND, g_print ("gdk_drag_context_drop_performed: 0x%p %u\n",
                          context,
                          time_));
  gdk_drag_drop (context, time_);
  drag_context_ungrab (context);
}

#define BIG_STEP 20
#define SMALL_STEP 1

static void
gdk_drag_get_current_actions (GdkModifierType  state,
                              gint             button,
                              GdkDragAction    actions,
                              GdkDragAction   *suggested_action,
                              GdkDragAction   *possible_actions)
{
  *suggested_action = 0;
  *possible_actions = 0;

  if ((button == GDK_BUTTON_MIDDLE || button == GDK_BUTTON_SECONDARY) && (actions & GDK_ACTION_ASK))
    {
      *suggested_action = GDK_ACTION_ASK;
      *possible_actions = actions;
    }
  else if (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
    {
      if ((state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK))
        {
          if (actions & GDK_ACTION_LINK)
            {
              *suggested_action = GDK_ACTION_LINK;
              *possible_actions = GDK_ACTION_LINK;
            }
        }
      else if (state & GDK_CONTROL_MASK)
        {
          if (actions & GDK_ACTION_COPY)
            {
              *suggested_action = GDK_ACTION_COPY;
              *possible_actions = GDK_ACTION_COPY;
            }
        }
      else
        {
          if (actions & GDK_ACTION_MOVE)
            {
              *suggested_action = GDK_ACTION_MOVE;
              *possible_actions = GDK_ACTION_MOVE;
            }
        }
    }
  else
    {
      *possible_actions = actions;

      if ((state & (GDK_MOD1_MASK)) && (actions & GDK_ACTION_ASK))
        *suggested_action = GDK_ACTION_ASK;
      else if (actions & GDK_ACTION_COPY)
        *suggested_action =  GDK_ACTION_COPY;
      else if (actions & GDK_ACTION_MOVE)
        *suggested_action = GDK_ACTION_MOVE;
      else if (actions & GDK_ACTION_LINK)
        *suggested_action = GDK_ACTION_LINK;
    }
}

static void
gdk_drag_update (GdkDragContext  *context,
                 gdouble          x_root,
                 gdouble          y_root,
                 GdkModifierType  mods,
                 guint32          evtime)
{
  GdkWin32DragContext *win32_context = GDK_WIN32_DRAG_CONTEXT (context);
  GdkDragAction action, possible_actions;
  GdkSurface *dest_surface;
  GdkDragProtocol protocol;

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  gdk_drag_get_current_actions (mods, GDK_BUTTON_PRIMARY, win32_context->actions,
                                &action, &possible_actions);

  dest_surface = gdk_win32_drag_context_find_surface (context,
                                                      win32_context->drag_surface,
                                                      x_root, y_root, &protocol);

  gdk_win32_drag_context_drag_motion (context, dest_surface, protocol, x_root, y_root,
                                      action, possible_actions, evtime);
}

static gboolean
gdk_dnd_handle_motion_event (GdkDragContext       *context,
                             const GdkEventMotion *event)
{
  GdkModifierType state;

  if (!gdk_event_get_state ((GdkEvent *) event, &state))
    return FALSE;

  GDK_NOTE (DND, g_print ("gdk_dnd_handle_motion_event: 0x%p\n",
                          context));

  gdk_drag_update (context, event->x_root, event->y_root, state,
                   gdk_event_get_time ((GdkEvent *) event));


  if (use_ole2_dnd)
    {
      GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
      GdkWin32DragContext *context_win32;
      DWORD key_state = 0;
      BYTE kbd_state[256];

      /* TODO: is this correct? We get the state at current moment,
       * not at the moment when the event was emitted.
       * I don't think that it ultimately serves any purpose,
       * as our IDropSource does not react to the keyboard
       * state changes (rather, it reacts to our status updates),
       * but there's no way to tell what goes inside DoDragDrop(),
       * so we should send at least *something* that looks right.
       */
      API_CALL (GetKeyboardState, (kbd_state));

      if (kbd_state[VK_CONTROL] & 0x80)
        key_state |= MK_CONTROL;
      if (kbd_state[VK_SHIFT] & 0x80)
        key_state |= MK_SHIFT;
      if (kbd_state[VK_LBUTTON] & 0x80)
        key_state |= MK_LBUTTON;
      if (kbd_state[VK_MBUTTON] & 0x80)
        key_state |= MK_MBUTTON;
      if (kbd_state[VK_RBUTTON] & 0x80)
        key_state |= MK_RBUTTON;

      GDK_NOTE (DND, g_print ("Post WM_MOUSEMOVE keystate=%lu\n", key_state));

      context_win32 = GDK_WIN32_DRAG_CONTEXT (context);

      context_win32->util_data.last_x = event->x_root;
      context_win32->util_data.last_y = event->y_root;

      API_CALL (PostThreadMessage, (clipdrop->dnd_thread_id,
                                    WM_MOUSEMOVE,
                                    key_state,
                                    MAKELPARAM ((event->x_root - _gdk_offset_x) * context_win32->scale,
                                                (event->y_root - _gdk_offset_y) * context_win32->scale)));
    }

  return TRUE;
}

static gboolean
gdk_dnd_handle_key_event (GdkDragContext    *context,
                          const GdkEventKey *event)
{
  GdkWin32DragContext *win32_context = GDK_WIN32_DRAG_CONTEXT (context);
  GdkModifierType state;
  GdkDevice *pointer;
  gint dx, dy;

  GDK_NOTE (DND, g_print ("gdk_dnd_handle_key_event: 0x%p\n",
                          context));

  dx = dy = 0;
  state = event->state;
  pointer = gdk_device_get_associated_device (gdk_event_get_device ((GdkEvent *) event));

  if (event->any.type == GDK_KEY_PRESS)
    {
      switch (event->keyval)
        {
        case GDK_KEY_Escape:
          gdk_drag_context_cancel (context, GDK_DRAG_CANCEL_USER_CANCELLED);
          return TRUE;

        case GDK_KEY_space:
        case GDK_KEY_Return:
        case GDK_KEY_ISO_Enter:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_KP_Space:
          if ((gdk_drag_context_get_selected_action (context) != 0) &&
              (gdk_drag_context_get_dest_surface (context) != NULL))
            {
              g_signal_emit_by_name (context, "drop-performed",
                                     gdk_event_get_time ((GdkEvent *) event));
            }
          else
            gdk_drag_context_cancel (context, GDK_DRAG_CANCEL_NO_TARGET);

          return TRUE;

        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
          dy = (state & GDK_MOD1_MASK) ? -BIG_STEP : -SMALL_STEP;
          break;

        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
          dy = (state & GDK_MOD1_MASK) ? BIG_STEP : SMALL_STEP;
          break;

        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
          dx = (state & GDK_MOD1_MASK) ? -BIG_STEP : -SMALL_STEP;
          break;

        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
          dx = (state & GDK_MOD1_MASK) ? BIG_STEP : SMALL_STEP;
          break;
        }
    }

  /* The state is not yet updated in the event, so we need
   * to query it here.
   */
  _gdk_device_query_state (pointer, NULL, NULL, NULL, NULL, NULL, NULL, &state);

  if (dx != 0 || dy != 0)
    {
      win32_context->util_data.last_x += dx;
      win32_context->util_data.last_y += dy;
      gdk_device_warp (pointer, win32_context->util_data.last_x, win32_context->util_data.last_y);
    }

  gdk_drag_update (context, win32_context->util_data.last_x, win32_context->util_data.last_y, state,
                   gdk_event_get_time ((GdkEvent *) event));

  return TRUE;
}

static gboolean
gdk_dnd_handle_grab_broken_event (GdkDragContext           *context,
                                  const GdkEventGrabBroken *event)
{
  GdkWin32DragContext *win32_context = GDK_WIN32_DRAG_CONTEXT (context);

  GDK_NOTE (DND, g_print ("gdk_dnd_handle_grab_broken_event: 0x%p\n",
                          context));

  /* Don't cancel if we break the implicit grab from the initial button_press.
   * Also, don't cancel if we re-grab on the widget or on our IPC window, for
   * example, when changing the drag cursor.
   */
  if (event->implicit ||
      event->grab_surface == win32_context->drag_surface ||
      event->grab_surface == win32_context->ipc_window)
    return FALSE;

  if (gdk_event_get_device ((GdkEvent *) event) !=
      gdk_drag_context_get_device (context))
    return FALSE;

  gdk_drag_context_cancel (context, GDK_DRAG_CANCEL_ERROR);
  return TRUE;
}

static gboolean
gdk_dnd_handle_button_event (GdkDragContext       *context,
                             const GdkEventButton *event)
{
  GDK_NOTE (DND, g_print ("gdk_dnd_handle_button_event: 0x%p\n",
                          context));

#if 0
  /* FIXME: Check the button matches */
  if (event->button != win32_context->button)
    return FALSE;
#endif

  if ((gdk_drag_context_get_selected_action (context) != 0))
    {
      g_signal_emit_by_name (context, "drop-performed",
                             gdk_event_get_time ((GdkEvent *) event));
    }
  else
    gdk_drag_context_cancel (context, GDK_DRAG_CANCEL_NO_TARGET);

  return TRUE;
}

gboolean
gdk_win32_drag_context_handle_event (GdkDragContext *context,
                                     const GdkEvent *event)
{
  GdkWin32DragContext *win32_context = GDK_WIN32_DRAG_CONTEXT (context);

  if (!context->is_source)
    return FALSE;
  if (!win32_context->grab_seat)
    return FALSE;

  switch (event->any.type)
    {
    case GDK_MOTION_NOTIFY:
      return gdk_dnd_handle_motion_event (context, &event->motion);
    case GDK_BUTTON_RELEASE:
      return gdk_dnd_handle_button_event (context, &event->button);
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      return gdk_dnd_handle_key_event (context, &event->key);
    case GDK_GRAB_BROKEN:
      return gdk_dnd_handle_grab_broken_event (context, &event->grab_broken);
    default:
      break;
    }

  return FALSE;
}

void
gdk_win32_drag_context_action_changed (GdkDragContext *context,
                                       GdkDragAction   action)
{
  GdkCursor *cursor;

  cursor = gdk_drag_get_cursor (context, action);
  gdk_drag_context_set_cursor (context, cursor);
}

static GdkSurface *
gdk_win32_drag_context_get_drag_surface (GdkDragContext *context)
{
  return GDK_WIN32_DRAG_CONTEXT (context)->drag_surface;
}

static void
gdk_win32_drag_context_set_hotspot (GdkDragContext *context,
                                    gint            hot_x,
                                    gint            hot_y)
{
  GdkWin32DragContext *win32_context = GDK_WIN32_DRAG_CONTEXT (context);

  GDK_NOTE (DND, g_print ("gdk_drag_context_set_hotspot: 0x%p %d:%d\n",
                          context,
                          hot_x, hot_y));

  win32_context->hot_x = hot_x;
  win32_context->hot_y = hot_y;

  if (win32_context->grab_seat)
    {
      /* DnD is managed, update current position */
      move_drag_surface (context, win32_context->util_data.last_x, win32_context->util_data.last_y);
    }
}

static void
gdk_win32_drag_context_class_init (GdkWin32DragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragContextClass *context_class = GDK_DRAG_CONTEXT_CLASS (klass);

  object_class->finalize = gdk_win32_drag_context_finalize;

  context_class->drag_abort = gdk_win32_drag_context_drag_abort;
  context_class->drag_drop = gdk_win32_drag_context_drag_drop;

  context_class->get_drag_surface = gdk_win32_drag_context_get_drag_surface;
  context_class->set_hotspot = gdk_win32_drag_context_set_hotspot;
  context_class->drop_done = gdk_win32_drag_context_drop_done;
  context_class->set_cursor = gdk_win32_drag_context_set_cursor;
  context_class->cancel = gdk_win32_drag_context_cancel;
  context_class->drop_performed = gdk_win32_drag_context_drop_performed;
  context_class->handle_event = gdk_win32_drag_context_handle_event;
  context_class->action_changed = gdk_win32_drag_context_action_changed;

}
