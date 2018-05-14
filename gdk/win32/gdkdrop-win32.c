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

typedef struct
{
  IDropTarget                     idt;
  gint                            ref_count;
  GdkDragContext                 *context;
  /* We get this at the object creation time and keep
   * it indefinitely. Contexts come and go, but this window
   * remains the same.
   */
  GdkSurface                     *dest_surface;
  /* This is given to us by the OS, we store it here
   * until the drag leaves our window.
   */
  IDataObject                    *data_object;
} target_drag_context;

static GList *dnd_target_contexts;
static GdkDragContext *current_dest_drag = NULL;

static gboolean use_ole2_dnd = TRUE;

G_DEFINE_TYPE (GdkWin32DropContext, gdk_win32_drop_context, GDK_TYPE_DRAG_CONTEXT)

static void
gdk_win32_drop_context_init (GdkWin32DropContext *context)
{
  if (!use_ole2_dnd)
    {
      dnd_target_contexts = g_list_prepend (dnd_target_contexts, context);
    }
  else
    {
    }

  GDK_NOTE (DND, g_print ("gdk_drop_context_init %p\n", context));
}

static void
gdk_win32_drop_context_finalize (GObject *object)
{
  GdkDragContext *context;
  GdkWin32DropContext *context_win32;

  GDK_NOTE (DND, g_print ("gdk_drop_context_finalize %p\n", object));

  g_return_if_fail (GDK_IS_WIN32_DROP_CONTEXT (object));

  context = GDK_DRAG_CONTEXT (object);
  context_win32 = GDK_WIN32_DROP_CONTEXT (context);

  if (!use_ole2_dnd)
    {
      dnd_target_contexts = g_list_remove (dnd_target_contexts, context);

      if (context == current_dest_drag)
        current_dest_drag = NULL;
    }

  g_clear_object (&context_win32->local_source_context);

  g_array_unref (context_win32->droptarget_w32format_contentformat_map);

  G_OBJECT_CLASS (gdk_win32_drop_context_parent_class)->finalize (object);
}

/* Drag Contexts */

static GdkDragContext *
gdk_drop_context_new (GdkDisplay        *display,
                      GdkSurface        *source_surface,
                      GdkSurface        *dest_surface,
                      GdkContentFormats *formats,
                      GdkDragAction      actions,
                      GdkDragProtocol    protocol)
{
  GdkWin32DropContext *context_win32;
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);
  GdkDragContext *context;

  context_win32 = g_object_new (GDK_TYPE_WIN32_DROP_CONTEXT,
                                "device", gdk_seat_get_pointer (gdk_display_get_default_seat (display)),
                                "formats", formats,
                                NULL);

  context = GDK_DRAG_CONTEXT (context_win32);

  if (win32_display->has_fixed_scale)
    context_win32->scale = win32_display->surface_scale;
  else
    context_win32->scale = _gdk_win32_display_get_monitor_scale_factor (win32_display, NULL, NULL, NULL);

  context_win32->droptarget_w32format_contentformat_map = g_array_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair));

  context->is_source = FALSE;
  g_set_object (&context->source_surface, source_surface);
  g_set_object (&context->dest_surface, dest_surface);
  gdk_drag_context_set_actions (context, actions, actions);
  context_win32->protocol = protocol;

  gdk_content_formats_unref (formats);

  return context;
}

GdkDragContext *
_gdk_win32_drop_context_find (GdkSurface *source,
                              GdkSurface *dest)
{
  GList *tmp_list = dnd_target_contexts;
  GdkDragContext *context;

  while (tmp_list)
    {
      context = (GdkDragContext *) tmp_list->data;

      if (!context->is_source &&
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

/* map windows -> target drag contexts. The table
 * owns a ref to each context.
 */
static GHashTable* target_ctx_for_window = NULL;

static target_drag_context *
find_droptarget_for_target_context (GdkDragContext *context)
{
  return g_hash_table_lookup (target_ctx_for_window, GDK_SURFACE_HWND (context->dest_surface));
}

static ULONG STDMETHODCALLTYPE
idroptarget_addref (LPDROPTARGET This)
{
  target_drag_context *ctx = (target_drag_context *) This;

  int ref_count = ++ctx->ref_count;

  GDK_NOTE (DND, g_print ("idroptarget_addref %p %d\n", This, ref_count));

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_queryinterface (LPDROPTARGET This,
                            REFIID       riid,
                            LPVOID      *ppvObject)
{
  GDK_NOTE (DND, {
      g_print ("idroptarget_queryinterface %p ", This);
      PRINT_GUID (riid);
    });

  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      GDK_NOTE (DND, g_print ("...IUnknown S_OK\n"));
      idroptarget_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDropTarget))
    {
      GDK_NOTE (DND, g_print ("...IDropTarget S_OK\n"));
      idroptarget_addref (This);
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
idroptarget_release (LPDROPTARGET This)
{
  target_drag_context *ctx = (target_drag_context *) This;

  int ref_count = --ctx->ref_count;

  GDK_NOTE (DND, g_print ("idroptarget_release %p %d\n", This, ref_count));

  if (ref_count == 0)
    {
      g_object_unref (ctx->context);
      g_clear_object (&ctx->dest_surface);
      g_free (This);
    }

  return ref_count;
}

static GdkDragAction
get_suggested_action (GdkWin32DropContext *win32_context,
                      DWORD                grfKeyState)
{
  /* This is the yucky Windows standard: Force link action if both
   * Control and Alt are down, copy if Control is down alone, move if
   * Alt is down alone, or use default of move within the app or copy
   * when origin of the drag is in another app.
   */
  if (grfKeyState & MK_CONTROL && grfKeyState & MK_SHIFT)
    return GDK_ACTION_LINK; /* Link action not supported */
  else if (grfKeyState & MK_CONTROL)
    return GDK_ACTION_COPY;
  else if (grfKeyState & MK_ALT)
    return GDK_ACTION_MOVE;
  else if (win32_context->local_source_context &&
           win32_context->local_source_context->util_data.state == GDK_WIN32_DND_DRAGGING)
    return GDK_ACTION_MOVE;
  else
    return GDK_ACTION_COPY;
  /* Any way to determine when to add in DROPEFFECT_SCROLL? */
}

static DWORD
drop_effect_for_action (GdkDragAction action)
{
  DWORD effect = 0;

  if (action & GDK_ACTION_MOVE)
    effect |= DROPEFFECT_MOVE;
  if (action & GDK_ACTION_LINK)
    effect |= DROPEFFECT_LINK;
  if (action & GDK_ACTION_COPY)
    effect |= DROPEFFECT_COPY;

  if (effect == 0)
    effect = DROPEFFECT_NONE;

  return effect;
}

static void
dnd_event_emit (GdkEventType    type,
                GdkDragContext *context,
                gint            pt_x,
                gint            pt_y,
                GdkSurface     *dnd_surface)
{
  GdkEvent *e;

  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  e = gdk_event_new (type);

  e->any.send_event = FALSE;
  g_set_object (&e->dnd.context, context);
  g_set_object (&e->any.surface, dnd_surface);
  e->dnd.time = GDK_CURRENT_TIME;
  e->dnd.x_root = pt_x;
  e->dnd.y_root = pt_y;

  gdk_event_set_device (e, gdk_drag_context_get_device (context));

  GDK_NOTE (EVENTS, _gdk_win32_print_event (e));
  _gdk_event_emit (e);
  gdk_event_free (e);
}

static GdkContentFormats *
query_targets (LPDATAOBJECT  pDataObj,
               GArray       *format_target_map)
{
  IEnumFORMATETC *pfmt = NULL;
  FORMATETC fmt;
  GList *result = NULL;
  HRESULT hr;
  GdkContentFormatsBuilder *builder;
  GdkContentFormats *result_formats;
  GList *p;

  hr = IDataObject_EnumFormatEtc (pDataObj, DATADIR_GET, &pfmt);

  if (SUCCEEDED (hr))
    hr = IEnumFORMATETC_Next (pfmt, 1, &fmt, NULL);

  while (SUCCEEDED (hr) && hr != S_FALSE)
    {
      gboolean is_predef;
      gchar *registered_name = _gdk_win32_get_clipboard_format_name (fmt.cfFormat, &is_predef);

      if (registered_name && is_predef)
        GDK_NOTE (DND, g_print ("supported built-in source format 0x%x %s\n", fmt.cfFormat, registered_name));
      else if (registered_name)
        GDK_NOTE (DND, g_print ("supported source format 0x%x %s\n", fmt.cfFormat, registered_name));
      else
        GDK_NOTE (DND, g_print ("supported unnamed? source format 0x%x\n", fmt.cfFormat));

      g_free (registered_name);
      _gdk_win32_add_w32format_to_pairs (fmt.cfFormat, format_target_map, &result);
      hr = IEnumFORMATETC_Next (pfmt, 1, &fmt, NULL);
    }

  if (pfmt)
    IEnumFORMATETC_Release (pfmt);

  builder = gdk_content_formats_builder_new ();

  for (p = g_list_reverse (result); p; p = p->next)
    gdk_content_formats_builder_add_mime_type (builder, (const gchar *) p->data);

  result_formats = gdk_content_formats_builder_free_to_formats (builder);
  g_list_free (result);

  return result_formats;
}

static void
set_data_object (LPDATAOBJECT *location, LPDATAOBJECT data_object)
{
  if (*location != NULL)
    IDataObject_Release (*location);

  *location = data_object;

  if (*location != NULL)
    IDataObject_AddRef (*location);
}

static HRESULT STDMETHODCALLTYPE
idroptarget_dragenter (LPDROPTARGET This,
                       LPDATAOBJECT pDataObj,
                       DWORD        grfKeyState,
                       POINTL       pt,
                       LPDWORD      pdwEffect)
{
  target_drag_context *ctx = (target_drag_context *) This;
  GdkDragContext *context;
  GdkWin32DropContext *context_win32;
  GdkDisplay *display;
  gint pt_x;
  gint pt_y;
  GdkDragContext *source_context;

  GDK_NOTE (DND, g_print ("idroptarget_dragenter %p @ %ld : %ld for dest window 0x%p S_OK\n", This, pt.x, pt.y, ctx->dest_surface));

  g_clear_object (&ctx->context);

  source_context = _gdk_win32_find_source_context_for_dest_surface (ctx->dest_surface);

  display = gdk_surface_get_display (ctx->dest_surface);
  context = gdk_drop_context_new (display,
                                  /* OLE2 DnD does not allow us to get the source window,
                                   * but we *can* find it if it's ours. This is needed to
                                   * support DnD within the same widget, for example.
                                   */
                                  source_context ? source_context->source_surface : NULL,
                                  ctx->dest_surface,
                                  query_targets (pDataObj, context_win32->droptarget_w32format_contentformat_map),
                                  GDK_ACTION_COPY | GDK_ACTION_MOVE,
                                  GDK_DRAG_PROTO_OLE2);
  context_win32 = GDK_WIN32_DROP_CONTEXT (context);
  g_array_set_size (context_win32->droptarget_w32format_contentformat_map, 0);
  g_set_object (&context_win32->local_source_context, GDK_WIN32_DRAG_CONTEXT (source_context));

  ctx->context = context;
  context->action = GDK_ACTION_MOVE;
  gdk_drag_context_set_actions (context, 
                                GDK_ACTION_COPY | GDK_ACTION_MOVE,
                                get_suggested_action (context_win32, grfKeyState));
  set_data_object (&ctx->data_object, pDataObj);
  pt_x = pt.x / context_win32->scale + _gdk_offset_x;
  pt_y = pt.y / context_win32->scale + _gdk_offset_y;
  dnd_event_emit (GDK_DRAG_ENTER, context, pt_x, pt_y, context->dest_surface);
  dnd_event_emit (GDK_DRAG_MOTION, context, pt_x, pt_y, context->dest_surface);
  context_win32->last_key_state = grfKeyState;
  context_win32->last_x = pt_x;
  context_win32->last_y = pt_y;
  *pdwEffect = drop_effect_for_action (context->action);

  GDK_NOTE (DND, g_print ("idroptarget_dragenter returns with action %d and drop effect %lu\n", context->action, *pdwEffect));

  return S_OK;
}

/* NOTE: This method is called continuously, even if nothing is
 * happening, as long as the drag operation is in progress and
 * the cursor is above our window.
 * It is OK to return a "safe" dropeffect value (DROPEFFECT_NONE,
 * to indicate that the drop is not possible here), when we
 * do not yet have any real information about acceptability of
 * the drag, because we will have another opportunity to return
 * the "right" value (once we know what it is, after GTK processes
 * the events we emit) very soon.
 */
static HRESULT STDMETHODCALLTYPE
idroptarget_dragover (LPDROPTARGET This,
                      DWORD        grfKeyState,
                      POINTL       pt,
                      LPDWORD      pdwEffect)
{
  target_drag_context *ctx = (target_drag_context *) This;
  GdkWin32DropContext *context_win32 = GDK_WIN32_DROP_CONTEXT (ctx->context);
  gint pt_x = pt.x / context_win32->scale + _gdk_offset_x;
  gint pt_y = pt.y / context_win32->scale + _gdk_offset_y;

  gdk_drag_context_set_actions (ctx->context,
                                gdk_drag_context_get_actions (ctx->context),
                                get_suggested_action (context_win32, grfKeyState));

  GDK_NOTE (DND, g_print ("idroptarget_dragover %p @ %d : %d (raw %ld : %ld), suggests %d action S_OK\n", This, pt_x, pt_y, pt.x, pt.y, gdk_drag_context_get_suggested_action (ctx->context)));

  if (pt_x != context_win32->last_x ||
      pt_y != context_win32->last_y ||
      grfKeyState != context_win32->last_key_state)
    {
      dnd_event_emit (GDK_DRAG_MOTION, ctx->context, pt_x, pt_y, ctx->context->dest_surface);
      context_win32->last_key_state = grfKeyState;
      context_win32->last_x = pt_x;
      context_win32->last_y = pt_y;
    }

  *pdwEffect = drop_effect_for_action (ctx->context->action);

  GDK_NOTE (DND, g_print ("idroptarget_dragover returns with action %d and effect %lu\n", ctx->context->action, *pdwEffect));

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_dragleave (LPDROPTARGET This)
{
  target_drag_context *ctx = (target_drag_context *) This;

  GDK_NOTE (DND, g_print ("idroptarget_dragleave %p S_OK\n", This));

  dnd_event_emit (GDK_DRAG_LEAVE, ctx->context, 0, 0, ctx->context->dest_surface);

  g_clear_object (&ctx->context);
  set_data_object (&ctx->data_object, NULL);

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_drop (LPDROPTARGET This,
                  LPDATAOBJECT pDataObj,
                  DWORD        grfKeyState,
                  POINTL       pt,
                  LPDWORD      pdwEffect)
{
  target_drag_context *ctx = (target_drag_context *) This;
  GdkWin32DropContext *context_win32 = GDK_WIN32_DROP_CONTEXT (ctx->context);
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();
  gint pt_x = pt.x / context_win32->scale + _gdk_offset_x;
  gint pt_y = pt.y / context_win32->scale + _gdk_offset_y;

  GDK_NOTE (DND, g_print ("idroptarget_drop %p ", This));

  if (pDataObj == NULL)
    {
      GDK_NOTE (DND, g_print ("E_POINTER\n"));
      g_clear_object (&ctx->context);
      set_data_object (&ctx->data_object, NULL);

      return E_POINTER;
    }

  gdk_drag_context_set_actions (ctx->context,
                                gdk_drag_context_get_actions (ctx->context),
                                get_suggested_action (context_win32, grfKeyState));

  dnd_event_emit (GDK_DROP_START, ctx->context, pt_x, pt_y, ctx->context->dest_surface);

  /* Notify OLE of copy or move */
  *pdwEffect = drop_effect_for_action (ctx->context->action);

  g_clear_object (&ctx->context);
  set_data_object (&ctx->data_object, NULL);

  GDK_NOTE (DND, g_print ("drop S_OK with effect %lx\n", *pdwEffect));

  return S_OK;
}

static IDropTargetVtbl idt_vtbl = {
  idroptarget_queryinterface,
  idroptarget_addref,
  idroptarget_release,
  idroptarget_dragenter,
  idroptarget_dragover,
  idroptarget_dragleave,
  idroptarget_drop
};

static target_drag_context *
target_context_new (GdkSurface *window)
{
  target_drag_context *result;

  result = g_new0 (target_drag_context, 1);
  result->idt.lpVtbl = &idt_vtbl;
  result->ref_count = 0;

  result->dest_surface = g_object_ref (window);

  idroptarget_addref (&result->idt);

  GDK_NOTE (DND, g_print ("target_context_new: %p (window %p)\n", result, result->dest_surface));

  return result;
}

/* From MS Knowledge Base article Q130698 */

static gboolean
resolve_link (HWND     hWnd,
              wchar_t *link,
              gchar  **lpszPath)
{
  WIN32_FILE_ATTRIBUTE_DATA wfad;
  HRESULT hr;
  IShellLinkW *pslW = NULL;
  IPersistFile *ppf = NULL;

  /* Check if the file is empty first because IShellLink::Resolve for
   * some reason succeeds with an empty file and returns an empty
   * "link target". (#524151)
   */
    if (!GetFileAttributesExW (link, GetFileExInfoStandard, &wfad) ||
        (wfad.nFileSizeHigh == 0 && wfad.nFileSizeLow == 0))
      return FALSE;

  /* Assume failure to start with: */
  *lpszPath = 0;

  /* Call CoCreateInstance to obtain the IShellLink interface
   * pointer. This call fails if CoInitialize is not called, so it is
   * assumed that CoInitialize has been called.
   */

  hr = CoCreateInstance (&CLSID_ShellLink,
                         NULL,
                         CLSCTX_INPROC_SERVER,
                         &IID_IShellLinkW,
                         (LPVOID *)&pslW);

  if (SUCCEEDED (hr))
   {
     /* The IShellLink interface supports the IPersistFile
      * interface. Get an interface pointer to it.
      */
     hr = pslW->lpVtbl->QueryInterface (pslW,
                                        &IID_IPersistFile,
                                        (LPVOID *) &ppf);
   }

  if (SUCCEEDED (hr))
    {
      /* Load the file. */
      hr = ppf->lpVtbl->Load (ppf, link, STGM_READ);
    }

  if (SUCCEEDED (hr))
    {
      /* Resolve the link by calling the Resolve()
       * interface function.
       */
      hr = pslW->lpVtbl->Resolve (pslW, hWnd, SLR_ANY_MATCH | SLR_NO_UI);
    }

  if (SUCCEEDED (hr))
    {
      wchar_t wtarget[MAX_PATH];

      hr = pslW->lpVtbl->GetPath (pslW, wtarget, MAX_PATH, NULL, 0);
      if (SUCCEEDED (hr))
        *lpszPath = g_utf16_to_utf8 (wtarget, -1, NULL, NULL, NULL);
    }

  if (ppf)
    ppf->lpVtbl->Release (ppf);

  if (pslW)
    pslW->lpVtbl->Release (pslW);

  return SUCCEEDED (hr);
}

#if 0

/* Check for filenames like C:\Users\tml\AppData\Local\Temp\d5qtkvvs.bmp */
static gboolean
filename_looks_tempish (const char *filename)
{
  char *dirname;
  char *p;
  const char *q;
  gboolean retval = FALSE;

  dirname = g_path_get_dirname (filename);

  p = dirname;
  q = g_get_tmp_dir ();

  while (*p && *q &&
         ((G_IS_DIR_SEPARATOR (*p) && G_IS_DIR_SEPARATOR (*q)) ||
          g_ascii_tolower (*p) == g_ascii_tolower (*q)))
    p++, q++;

  if (!*p && !*q)
    retval = TRUE;

  g_free (dirname);

  return retval;
}

static gboolean
close_it (gpointer data)
{
  close (GPOINTER_TO_INT (data));

  return FALSE;
}

#endif

static GdkWin32MessageFilterReturn
gdk_dropfiles_filter (GdkWin32Display *display,
                      MSG             *msg,
                      gint            *ret_valp,
                      gpointer         data)
{
  GdkSurface *window;
  GdkDragContext *context;
  GdkWin32DropContext *context_win32;
  GdkEvent *event;
  GString *result;
  HANDLE hdrop;
  POINT pt;
  gint nfiles, i;
  gchar *fileName, *linkedFile;

  if (msg->message != WM_DROPFILES)
    return GDK_WIN32_MESSAGE_FILTER_CONTINUE;

      GDK_NOTE (DND, g_print ("WM_DROPFILES: %p\n", msg->hwnd));

      window = gdk_win32_handle_table_lookup (msg->hwnd);

      context = gdk_drop_context_new (GDK_DISPLAY (display),
                                      NULL,
                                      window,
                                      gdk_content_formats_new ((const char *[2]) {
                                                                 "text/uri-list",
                                                                 NULL
                                                               }, 1),
                                      GDK_ACTION_COPY,
                                      GDK_DRAG_PROTO_WIN32_DROPFILES);
      context_win32 = GDK_WIN32_DROP_CONTEXT (context);
      /* WM_DROPFILES drops are always file names */

      gdk_drag_context_set_actions (context, GDK_ACTION_COPY, GDK_ACTION_COPY);
      current_dest_drag = context;

      hdrop = (HANDLE) msg->wParam;
      DragQueryPoint (hdrop, &pt);
      ClientToScreen (msg->hwnd, &pt);

      event = gdk_event_new (GDK_DROP_START);

      event->any.send_event = FALSE;
      g_set_object (&event->dnd.context, context);
      g_set_object (&event->any.surface, window);
      gdk_event_set_display (event, display);
      gdk_event_set_device (event, gdk_drag_context_get_device (context));
      event->dnd.x_root = pt.x / context_win32->scale + _gdk_offset_x;
      event->dnd.y_root = pt.y / context_win32->scale + _gdk_offset_y;
      event->dnd.time = _gdk_win32_get_next_tick (msg->time);

      nfiles = DragQueryFile (hdrop, 0xFFFFFFFF, NULL, 0);

      result = g_string_new (NULL);
      for (i = 0; i < nfiles; i++)
        {
          gchar *uri;
          wchar_t wfn[MAX_PATH];

          DragQueryFileW (hdrop, i, wfn, MAX_PATH);
          fileName = g_utf16_to_utf8 (wfn, -1, NULL, NULL, NULL);

          /* Resolve shortcuts */
          if (resolve_link (msg->hwnd, wfn, &linkedFile))
            {
              uri = g_filename_to_uri (linkedFile, NULL, NULL);
              if (uri != NULL)
                {
                  g_string_append (result, uri);
                  GDK_NOTE (DND, g_print ("... %s link to %s: %s\n",
                                          fileName, linkedFile, uri));
                  g_free (uri);
                }
              g_free (fileName);
              fileName = linkedFile;
            }
          else
            {
              uri = g_filename_to_uri (fileName, NULL, NULL);
              if (uri != NULL)
                {
                  g_string_append (result, uri);
                  GDK_NOTE (DND, g_print ("... %s: %s\n", fileName, uri));
                  g_free (uri);
                }
            }

#if 0
          /* Awful hack to recognize temp files corresponding to
           * images dragged from Firefox... Open the file right here
           * so that it is less likely that Firefox manages to delete
           * it before the GTK+-using app (typically GIMP) has opened
           * it.
           *
           * Not compiled in for now, because it means images dragged
           * from Firefox would stay around in the temp folder which
           * is not what Firefox intended. I don't feel comfortable
           * with that, both from a geenral sanity point of view, and
           * from a privacy point of view. It's better to wait for
           * Firefox to fix the problem, for instance by deleting the
           * temp file after a longer delay, or to wait until we
           * implement the OLE2_DND...
           */
          if (filename_looks_tempish (fileName))
            {
              int fd = g_open (fileName, _O_RDONLY|_O_BINARY, 0);
              if (fd == -1)
                {
                  GDK_NOTE (DND, g_print ("Could not open %s, maybe an image dragged from Firefox that it already deleted\n", fileName));
                }
              else
                {
                  GDK_NOTE (DND, g_print ("Opened %s as %d so that Firefox won't delete it\n", fileName, fd));
                  g_timeout_add_seconds (1, close_it, GINT_TO_POINTER (fd));
                }
            }
#endif

          g_free (fileName);
          g_string_append (result, "\015\012");
        }

      /* FIXME: this call is currently a no-op, but it should
       * stash the string somewhere, and later produce it,
       * maybe in response to gdk_win32_drop_context_read_async()?
       */
      _gdk_dropfiles_store (result->str);
      g_string_free (result, FALSE);

      GDK_NOTE (EVENTS, _gdk_win32_print_event (event));
      _gdk_event_emit (event);
      gdk_event_free (event);

      DragFinish (hdrop);

  gdk_display_put_event (display, event);
  gdk_event_free (event);

  *ret_valp = 0;

  return GDK_WIN32_MESSAGE_FILTER_REMOVE;
}

/* Destination side */

static void
gdk_win32_drop_context_status (GdkDrop       *drop,
                               GdkDragAction  action)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (drop);
  GdkDragContext *src_context;

  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drag_status: %s\n"
                          " context=%p:{actions=%s,suggested=%s,action=%s}\n",
                          _gdk_win32_drag_action_to_string (action),
                          context,
                          _gdk_win32_drag_action_to_string (gdk_drag_context_get_actions (context)),
                          _gdk_win32_drag_action_to_string (gdk_drag_context_get_suggested_action (context)),
                          _gdk_win32_drag_action_to_string (context->action)));

  context->action = action;

  if (!use_ole2_dnd)
    {
      src_context = _gdk_win32_drag_context_find (context->source_surface,
                                                  context->dest_surface);

      if (src_context)
        {
          _gdk_win32_drag_context_send_local_status_event (src_context, action);
        }
    }
}

static void
_gdk_display_put_event (GdkDisplay *display,
                        GdkEvent   *event)
{
  g_assert (_win32_main_thread == NULL ||
            _win32_main_thread == g_thread_self ());

  gdk_event_set_display (event, display);
  gdk_display_put_event (display, event);
}

static void
gdk_win32_drop_context_drop_finish (GdkDragContext *context,
                 gboolean        success,
                 guint32         time)
{
  GdkDragContext *src_context;
  GdkEvent *tmp_event;
  GdkWin32Clipdrop *clipdrop = _gdk_win32_clipdrop_get ();

  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drag_finish\n"));

  if (!use_ole2_dnd)
    {
      src_context = _gdk_win32_drag_context_find (context->source_surface,
                                                  context->dest_surface);
      if (src_context)
        {
          GDK_NOTE (DND, g_print ("gdk_dnd_handle_drop_finihsed: 0x%p\n",
                                  context));

          g_signal_emit_by_name (context, "dnd-finished");
          gdk_drag_drop_done (context, !GDK_WIN32_DROP_CONTEXT (context)->drop_failed);
        }
    }
  else
    {
      _gdk_win32_drag_do_leave (context, time);

      if (success)
        clipdrop->dnd_target_state = GDK_WIN32_DND_DROPPED;
      else
        clipdrop->dnd_target_state = GDK_WIN32_DND_FAILED;
    }
}

#if 0

static GdkFilterReturn
gdk_destroy_filter (GdkXEvent *xev,
                    GdkEvent  *event,
                    gpointer   data)
{
  MSG *msg = (MSG *) xev;

  if (msg->message == WM_DESTROY)
    {
      IDropTarget *idtp = (IDropTarget *) data;

      GDK_NOTE (DND, g_print ("gdk_destroy_filter: WM_DESTROY: %p\n", msg->hwnd));
#if 0
      idtp->lpVtbl->Release (idtp);
#endif
      RevokeDragDrop (msg->hwnd);
      CoLockObjectExternal ((IUnknown*) idtp, FALSE, TRUE);
    }
  return GDK_FILTER_CONTINUE;
}

#endif

void
_gdk_win32_surface_register_dnd (GdkSurface *window)
{
  target_drag_context *ctx;
  HRESULT hr;

  g_return_if_fail (window != NULL);

  if (g_object_get_data (G_OBJECT (window), "gdk-dnd-registered") != NULL)
    return;
  else
    g_object_set_data (G_OBJECT (window), "gdk-dnd-registered", GINT_TO_POINTER (TRUE));

  GDK_NOTE (DND, g_print ("gdk_surface_register_dnd: %p\n", GDK_SURFACE_HWND (window)));

  if (!use_ole2_dnd)
    {
      /* We always claim to accept dropped files, but in fact we might not,
       * of course. This function is called in such a way that it cannot know
       * whether the window (widget) in question actually accepts files
       * (in gtk, data of type text/uri-list) or not.
       */
      gdk_win32_display_add_filter (gdk_display_get_default (), gdk_dropfiles_filter, NULL);
      DragAcceptFiles (GDK_SURFACE_HWND (window), TRUE);
    }
  else
    {
      /* Return if window is already setup for DND. */
      if (g_hash_table_lookup (target_ctx_for_window, GDK_SURFACE_HWND (window)) != NULL)
        return;

      ctx = target_context_new (window);

      hr = CoLockObjectExternal ((IUnknown *) &ctx->idt, TRUE, FALSE);
      if (!SUCCEEDED (hr))
        OTHER_API_FAILED ("CoLockObjectExternal");
      else
        {
          hr = RegisterDragDrop (GDK_SURFACE_HWND (window), &ctx->idt);
          if (hr == DRAGDROP_E_ALREADYREGISTERED)
            {
              g_print ("DRAGDROP_E_ALREADYREGISTERED\n");
              CoLockObjectExternal ((IUnknown *) &ctx->idt, FALSE, FALSE);
            }
          else if (!SUCCEEDED (hr))
            OTHER_API_FAILED ("RegisterDragDrop");
          else
            {
              g_object_ref (window);
              g_hash_table_insert (target_ctx_for_window, GDK_SURFACE_HWND (window), ctx);
            }
        }
    }
}

static gpointer
grab_data_from_hdata (GTask  *task,
                      HANDLE  hdata,
                      gsize  *data_len)
{
  gpointer ptr;
  SIZE_T length;
  guchar *data;

  ptr = GlobalLock (hdata);
  if (ptr == NULL)
    {
      DWORD error_code = GetLastError ();
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Cannot get DnD data. GlobalLock(0x%p) failed: 0x%lx."), hdata, error_code);
      return NULL;
    }

  length = GlobalSize (hdata);
  if (length == 0 && GetLastError () != NO_ERROR)
    {
      DWORD error_code = GetLastError ();
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Cannot get DnD data. GlobalSize(0x%p) failed: 0x%lx."), hdata, error_code);
      GlobalUnlock (hdata);
      return NULL;
    }

  data = g_try_malloc (length);

  if (data == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Cannot get DnD data. Failed to allocate %lu bytes to store the data."), length);
      GlobalUnlock (hdata);
      return NULL;
    }

  memcpy (data, ptr, length);
  *data_len = length;

  GlobalUnlock (hdata);

  return data;
}

static void
gdk_win32_drop_context_read_async (GdkDrop             *drop,
                                   GdkContentFormats   *formats,
                                   int                  io_priority,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GdkWin32DropContext       *context_win32 = GDK_WIN32_DROP_CONTEXT (drop);
  GTask                     *task;
  target_drag_context       *tctx;
  const char * const        *mime_types;
  gsize                      i, j, n_mime_types;
  GdkWin32ContentFormatPair *pair;
  FORMATETC                  fmt;
  HRESULT                    hr;
  STGMEDIUM                  storage;
  guchar                    *data;
  gsize                      data_len;
  GInputStream              *stream;

  task = g_task_new (drop, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_win32_drop_context_read_async);

  tctx = find_droptarget_for_target_context (GDK_DRAG_CONTEXT (drop));

  if (tctx == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Failed to find target context record for context 0x%p"), drop);
      return;
    }

  if (tctx->data_object == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Target context record 0x%p has no data object"), tctx);
      return;
    }

  mime_types = gdk_content_formats_get_mime_types (formats, &n_mime_types);

  for (pair = NULL, i = 0; i < n_mime_types; i++)
    {
      for (j = 0; j < context_win32->droptarget_w32format_contentformat_map->len; j++)
        {
          pair = &g_array_index (context_win32->droptarget_w32format_contentformat_map, GdkWin32ContentFormatPair, j);
          if (pair->contentformat == mime_types[i])
            break;

          pair = NULL;
        }
    }

  if (pair == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("No compatible transfer format found"));
      return;
    }

  fmt.cfFormat = pair->w32format;
  if (_gdk_win32_format_uses_hdata (pair->w32format))
    fmt.tymed = TYMED_HGLOBAL;
  else
    g_assert_not_reached ();

  fmt.ptd = NULL;
  fmt.dwAspect = DVASPECT_CONTENT;
  fmt.lindex = -1;

  hr = IDataObject_GetData (tctx->data_object, &fmt, &storage);

  if (!SUCCEEDED (hr) || hr != S_OK)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("IDataObject_GetData (0x%x) failed, returning 0x%lx"), fmt.cfFormat, hr);
      return;
    }

  if (!pair->transmute)
    {
      if (_gdk_win32_format_uses_hdata (pair->w32format))
        {
          data = grab_data_from_hdata (task, storage.hGlobal, &data_len);

          if (data == NULL)
            {
              ReleaseStgMedium (&storage);

              return;
            }
        }
      else
        {
          g_assert_not_reached ();
        }
    }
  else
    {
      _gdk_win32_transmute_windows_data (pair->w32format, pair->contentformat, storage.hGlobal, &data, &data_len);
    }

  ReleaseStgMedium (&storage);

  if (data == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Failed to transmute DnD data W32 format 0x%x to %p (%s)"), pair->w32format, pair->contentformat, pair->contentformat);
      return;
    }

  stream = g_memory_input_stream_new_from_data (data, data_len, g_free);
  g_object_set_data (G_OBJECT (stream), "gdk-dnd-stream-contenttype", (gpointer) pair->contentformat);
  g_task_return_pointer (task, stream, g_object_unref);
}

static GInputStream *
gdk_win32_drop_context_read_finish (GdkDrop       *drop,
                                    const char   **out_mime_type,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  GTask *task;
  GInputStream *stream;

  g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (drop)), NULL);
  task = G_TASK (result);
  g_return_val_if_fail (g_task_get_source_tag (task) == gdk_win32_drop_context_read_async, NULL);

  stream = g_task_propagate_pointer (task, error);

  if (stream && out_mime_type)
    *out_mime_type = g_object_get_data (G_OBJECT (stream), "gdk-dnd-stream-contenttype");

  return stream;
}

static void
gdk_win32_drop_context_class_init (GdkWin32DropContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDropClass *drop_class = GDK_DROP_CLASS (klass);
  GdkDragContextClass *context_class = GDK_DRAG_CONTEXT_CLASS (klass);

  object_class->finalize = gdk_win32_drop_context_finalize;

  drop_class->status = gdk_win32_drop_context_status;
  drop_class->read_async = gdk_win32_drop_context_read_async;
  drop_class->read_finish = gdk_win32_drop_context_read_finish;

  context_class->drop_finish = gdk_win32_drop_context_drop_finish;
}

void
_gdk_win32_local_send_enter (GdkDragContext *context,
                             guint32         time)
{
  GdkEvent *tmp_event;
  GdkDragContext *new_context;

  GDK_NOTE (DND, g_print ("local_send_enter: context=%p current_dest_drag=%p\n",
                          context,
                          current_dest_drag));

  if (current_dest_drag != NULL)
    {
      g_object_unref (G_OBJECT (current_dest_drag));
      current_dest_drag = NULL;
    }

  new_context = gdk_drop_context_new (gdk_surface_get_display (context->source_surface),
                                      context->source_surface,
                                      context->dest_surface,
                                      gdk_content_formats_ref (gdk_drag_context_get_formats (context)),
                                      gdk_drag_context_get_actions (context),
                                      GDK_DRAG_PROTO_LOCAL);

  gdk_surface_set_events (new_context->source_surface,
                          gdk_surface_get_events (new_context->source_surface) |
                          GDK_PROPERTY_CHANGE_MASK);

  tmp_event = gdk_event_new (GDK_DRAG_ENTER);
  g_set_object (&tmp_event->any.surface, context->dest_surface);
  tmp_event->any.send_event = FALSE;
  g_set_object (&tmp_event->dnd.context, new_context);
  tmp_event->dnd.time = GDK_CURRENT_TIME; /* FIXME? */
  gdk_event_set_device (tmp_event, gdk_drag_context_get_device (context));

  current_dest_drag = new_context;

  GDK_NOTE (EVENTS, _gdk_win32_print_event (tmp_event));
  _gdk_display_put_event (gdk_device_get_display (gdk_drag_context_get_device (context)), tmp_event);
  gdk_event_free (tmp_event);
}

void
_gdk_drop_init (void)
{
  if (use_ole2_dnd)
    {
      target_ctx_for_window = g_hash_table_new (g_direct_hash, g_direct_equal);
    }
}
