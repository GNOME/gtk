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

#include "gdkdropprivate.h"

#include "gdkdrag.h"
#include "gdkproperty.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"
#include "gdkwin32.h"
#include "gdkwin32dnd.h"
#include "gdkdisplayprivate.h"
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

#define GDK_TYPE_WIN32_DROP              (gdk_win32_drop_get_type ())
#define GDK_WIN32_DROP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WIN32_DROP, GdkWin32Drop))
#define GDK_WIN32_DROP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WIN32_DROP, GdkWin32DropClass))
#define GDK_IS_WIN32_DROP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WIN32_DROP))
#define GDK_IS_WIN32_DROP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WIN32_DROP))
#define GDK_WIN32_DROP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WIN32_DROP, GdkWin32DropClass))

typedef struct _GdkWin32Drop GdkWin32Drop;
typedef struct _GdkWin32DropClass GdkWin32DropClass;

struct _GdkWin32Drop
{
  GdkDrop          drop;

  /* The drag protocol being in use */
  GdkDragProtocol  protocol;

  /* The actions supported at GTK level. Set in gdk_win32_drop_status(). */
  GdkDragAction    actions;

  guint            scale;          /* Temporarily caches the HiDPI scale */
  gint             last_x;         /* Coordinates from last event, in GDK space */
  gint             last_y;
  DWORD            last_key_state; /* Key state from last event */

  /* Just like GdkDrop->formats, but an array, and with format IDs
   * stored inside.
   */
  GArray          *droptarget_w32format_contentformat_map;

  /* The list from WM_DROPFILES is store here temporarily,
   * until the next gdk_win32_drop_read_async ()
   */
  gchar           *dropfiles_list;

  guint drop_finished : 1; /* FALSE until gdk_drop_finish() is called */
  guint drop_failed : 1;   /* Whether the drop was unsuccessful */
};

struct _GdkWin32DropClass
{
  GdkDropClass parent_class;
};

GType    gdk_win32_drop_get_type (void);

G_DEFINE_TYPE (GdkWin32Drop, gdk_win32_drop, GDK_TYPE_DROP)

/* This structure is presented to COM as an object that
 * implements IDropTarget interface. Every surface that
 * can be a droptarget has one of these.
 */
struct _drop_target_context
{
  IDropTarget                     idt;
  gint                            ref_count;

  /* The drop object we create when a drag enters our surface.
   * The drop object is destroyed when the drag leaves.
   */
  GdkDrop                        *drop;
  /* We get this at the object at creation time and keep
   * it indefinitely. Drops (see above) come and go, but
   * this surface remains the same.
   * This is not a reference, as drop_target_context must not
   * outlive the surface it's attached to.
   * drop_target_context is not folded into GdkWin32Surface
   * only because it's easier to present it to COM as a separate
   * object when it's allocated separately.
   */
  GdkSurface                     *surface;
  /* This is given to us by the OS, we store it here
   * until the drag leaves our window. It is referenced
   * (using COM reference counting).
   */
  IDataObject                    *data_object;
};

/* TRUE to use OLE2 protocol, FALSE to use local protocol */
static gboolean use_ole2_dnd = TRUE;

static void
gdk_win32_drop_init (GdkWin32Drop *drop)
{
  drop->droptarget_w32format_contentformat_map = g_array_new (FALSE, FALSE, sizeof (GdkWin32ContentFormatPair));

  GDK_NOTE (DND, g_print ("gdk_win32_drop_init %p\n", drop));
}

static void
gdk_win32_drop_finalize (GObject *object)
{
  GdkDrop *drop;
  GdkWin32Drop *drop_win32;

  GDK_NOTE (DND, g_print ("gdk_win32_drop_finalize %p\n", object));

  drop = GDK_DROP (object);

  drop_win32 = GDK_WIN32_DROP (drop);

  g_array_unref (drop_win32->droptarget_w32format_contentformat_map);

  G_OBJECT_CLASS (gdk_win32_drop_parent_class)->finalize (object);
}

static GdkDrop *
gdk_drop_new (GdkDisplay        *display,
              GdkDevice         *device,
              GdkDrag           *drag,
              GdkContentFormats *formats,
              GdkSurface        *surface,
              GdkDragProtocol    protocol)
{
  GdkWin32Drop *drop_win32;
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);

  drop_win32 = g_object_new (GDK_TYPE_WIN32_DROP,
                             "device", device,
                             "drag", drag,
                             "formats", formats,
                             "surface", surface,
                             NULL);

  if (win32_display->has_fixed_scale)
    drop_win32->scale = win32_display->surface_scale;
  else
    drop_win32->scale = _gdk_win32_display_get_monitor_scale_factor (win32_display, NULL, NULL, NULL);

  drop_win32->protocol = protocol;

  return GDK_DROP (drop_win32);
}

/* Gets the GdkDrop that corresponds to a particular GdkSurface.
 * Will be NULL for surfaces that are not registered as drop targets,
 * or for surfaces that are currently not under the drag cursor.
 * This function is only used for local DnD, where we do have
 * a real GdkSurface that corresponds to the HWND under cursor.
 */
GdkDrop *
_gdk_win32_get_drop_for_dest_surface (GdkSurface *dest)
{
  GdkWin32Surface *impl;

  if (dest == NULL)
    return NULL;

  impl = GDK_WIN32_SURFACE (dest);

  if (impl->drop_target != NULL)
    return impl->drop_target->drop;

  return impl->drop;
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

static ULONG STDMETHODCALLTYPE
idroptarget_addref (LPDROPTARGET This)
{
  drop_target_context *ctx = (drop_target_context *) This;

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
  drop_target_context *ctx = (drop_target_context *) This;

  int ref_count = --ctx->ref_count;

  GDK_NOTE (DND, g_print ("idroptarget_release %p %d\n", This, ref_count));

  if (ref_count == 0)
    {
      g_clear_object (&ctx->drop);
      g_free (This);
    }

  return ref_count;
}

static GdkContentFormats *
query_object_formats (LPDATAOBJECT  pDataObj,
                      GArray       *w32format_contentformat_map)
{
  IEnumFORMATETC *pfmt = NULL;
  FORMATETC fmt;
  HRESULT hr;
  GdkContentFormatsBuilder *builder;
  GdkContentFormats *result_formats;

  builder = gdk_content_formats_builder_new ();

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
      _gdk_win32_add_w32format_to_pairs (fmt.cfFormat, w32format_contentformat_map, builder);
      hr = IEnumFORMATETC_Next (pfmt, 1, &fmt, NULL);
    }

  if (pfmt)
    IEnumFORMATETC_Release (pfmt);

  result_formats = gdk_content_formats_builder_free_to_formats (builder);

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

/* Figures out an action that the user forces onto us by
 * pressing some modifier keys.
 */
static GdkDragAction
get_user_action (DWORD grfKeyState)
{
  /* Windows explorer does this:
   * 'C'ontrol for 'C'opy
   * a'L't (or Contro'L' + Shift) for 'L'ink
   * Shift for Move
   * Control + Alt or Shift + Alt or Control + Alt + Shift for Default action (see below).
   *
   * Default action is 'Copy' when dragging between drives, 'Move' otherwise.
   * For GTK 'between drives' turns into 'between applications'.
   */
  if (((grfKeyState & (MK_CONTROL | MK_ALT)) == (MK_CONTROL | MK_ALT)) ||
      ((grfKeyState & (MK_ALT | MK_SHIFT)) == (MK_ALT | MK_SHIFT)) ||
      ((grfKeyState & (MK_CONTROL | MK_ALT | MK_SHIFT)) == (MK_CONTROL | MK_ALT | MK_SHIFT)))
    {
      return 0;
    }
  else if ((grfKeyState & (MK_CONTROL | MK_SHIFT)) == (MK_CONTROL | MK_SHIFT))
    return GDK_ACTION_LINK;
  else if (grfKeyState & MK_CONTROL)
    return GDK_ACTION_COPY;
  else if (grfKeyState & MK_ALT)
    return GDK_ACTION_MOVE;

  return 0;
}

static DWORD
drop_effect_for_actions (GdkDragAction actions)
{
  DWORD effects = 0;
  int effect_count = 0;

  if (actions & GDK_ACTION_MOVE)
    {
      effects |= DROPEFFECT_MOVE;
      effect_count++;
    }
  if (actions & GDK_ACTION_LINK)
    {
      effects |= DROPEFFECT_LINK;
      effect_count++;
    }
  if (actions & GDK_ACTION_COPY)
    {
      effects |= DROPEFFECT_COPY;
      effect_count++;
    }

  if (effect_count == 0)
    effects = DROPEFFECT_NONE;
  else if (effect_count > 1)
    /* Actually it should be DROPEFECT_ASK, but Windows doesn't support that */
    effects = DROPEFFECT_COPY;

  return effects;
}

static GdkDragAction
actions_for_drop_effects (DWORD effects)
{
  GdkDragAction actions = 0;

  if (effects & DROPEFFECT_MOVE)
    actions |= GDK_ACTION_MOVE;
  if (effects & DROPEFFECT_LINK)
    actions |= GDK_ACTION_LINK;
  if (effects & DROPEFFECT_COPY)
    actions |= GDK_ACTION_COPY;

  return actions;
}

static GdkDragAction
filter_actions (GdkDragAction actions,
                GdkDragAction filter)
{
  return actions & filter;
}

static GdkDragAction
set_source_actions_helper (GdkDrop       *drop,
                           GdkDragAction  actions,
                           DWORD          grfKeyState)
{
  GdkDragAction user_action;

  user_action = get_user_action (grfKeyState);

  if (user_action != 0)
    gdk_drop_set_actions (drop, user_action);
  else
    gdk_drop_set_actions (drop, actions);

  return actions;
}

void
_gdk_win32_local_drop_target_dragenter (GdkDrag        *drag,
                                        GdkSurface     *dest_surface,
                                        gint            x_root,
                                        gint            y_root,
                                        DWORD           grfKeyState,
                                        guint32         time_,
                                        GdkDragAction  *actions)
{
  GdkDrop *drop;
  GdkWin32Drop *drop_win32;
  GdkDisplay *display;
  GdkDragAction source_actions;
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (dest_surface);

  GDK_NOTE (DND, g_print ("_gdk_win32_local_drop_target_dragenter %p @ %d : %d"
                          " for dest window 0x%p"
                          ". actions = %s\n",
                          drag, x_root, y_root,
                          dest_surface,
                          _gdk_win32_drag_action_to_string (*actions)));

  display = gdk_surface_get_display (dest_surface);
  drop = gdk_drop_new (display,
                       gdk_seat_get_pointer (gdk_display_get_default_seat (display)),
                       drag,
                       gdk_content_formats_ref (gdk_drag_get_formats (drag)),
                       dest_surface,
                       GDK_DRAG_PROTO_LOCAL);
  drop_win32 = GDK_WIN32_DROP (drop);

  impl->drop = drop;

  source_actions = set_source_actions_helper (drop, *actions, grfKeyState);

  gdk_drop_emit_enter_event (drop, TRUE, time_);
  gdk_drop_emit_motion_event (drop, TRUE, x_root, y_root, time_);
  drop_win32->last_key_state = grfKeyState;
  drop_win32->last_x = x_root;
  drop_win32->last_y = y_root;
  *actions = filter_actions (drop_win32->actions, source_actions);

  GDK_NOTE (DND, g_print ("_gdk_win32_local_drop_target_dragenter returns with actions %s\n",
                          _gdk_win32_drag_action_to_string (*actions)));
}

/* The pdwEffect here initially points
 * to a DWORD that contains the value of dwOKEffects argument in DoDragDrop,
 * i.e. the drag action that the drag source deems acceptable.
 * On return it should point to the effect value that denotes the
 * action that is going to happen on drop, and that is what DoDragDrop will
 * put into the DWORD that pdwEffect was pointing to.
 */
static HRESULT STDMETHODCALLTYPE
idroptarget_dragenter (LPDROPTARGET This,
                       LPDATAOBJECT pDataObj,
                       DWORD        grfKeyState,
                       POINTL       pt,
                       LPDWORD      pdwEffect_and_dwOKEffects)
{
  drop_target_context *ctx = (drop_target_context *) This;
  GdkDrop *drop;
  GdkWin32Drop *drop_win32;
  GdkDisplay *display;
  gint pt_x;
  gint pt_y;
  GdkDrag *drag;
  GdkDragAction source_actions;
  GdkDragAction dest_actions;

  GDK_NOTE (DND, g_print ("idroptarget_dragenter %p @ %ld : %ld"
                          " for dest window 0x%p"
                          ". dwOKEffects = %lu\n",
                          This, pt.x, pt.y,
                          ctx->surface,
                          *pdwEffect_and_dwOKEffects));

  g_clear_object (&ctx->drop);

  /* Try to find the GdkDrag object for this DnD operation,
   * if it originated in our own application.
   */
  drag = NULL;

  if (ctx->surface)
    drag = _gdk_win32_find_drag_for_dest_window (GDK_SURFACE_HWND (ctx->surface));

  display = gdk_surface_get_display (ctx->surface);
  drop = gdk_drop_new (display,
                       gdk_seat_get_pointer (gdk_display_get_default_seat (display)),
                       drag,
                       query_object_formats (pDataObj, NULL),
                       ctx->surface,
                       GDK_DRAG_PROTO_OLE2);
  drop_win32 = GDK_WIN32_DROP (drop);
  g_array_set_size (drop_win32->droptarget_w32format_contentformat_map, 0);
  gdk_content_formats_unref (query_object_formats (pDataObj, drop_win32->droptarget_w32format_contentformat_map));

  ctx->drop = drop;

  source_actions = set_source_actions_helper (drop,
                                              actions_for_drop_effects (*pdwEffect_and_dwOKEffects),
                                              grfKeyState);

  set_data_object (&ctx->data_object, pDataObj);
  pt_x = pt.x / drop_win32->scale + _gdk_offset_x;
  pt_y = pt.y / drop_win32->scale + _gdk_offset_y;
  gdk_drop_emit_enter_event (drop, TRUE, GDK_CURRENT_TIME);
  gdk_drop_emit_motion_event (drop, TRUE, pt_x, pt_y, GDK_CURRENT_TIME);
  drop_win32->last_key_state = grfKeyState;
  drop_win32->last_x = pt_x;
  drop_win32->last_y = pt_y;
  dest_actions = filter_actions (drop_win32->actions, source_actions);
  *pdwEffect_and_dwOKEffects = drop_effect_for_actions (dest_actions);

  GDK_NOTE (DND, g_print ("idroptarget_dragenter returns S_OK with actions %s"
                          " and drop effect %lu\n",
                          _gdk_win32_drag_action_to_string (dest_actions),
                          *pdwEffect_and_dwOKEffects));

  return S_OK;
}

gboolean
_gdk_win32_local_drop_target_will_emit_motion (GdkDrop *drop,
                                               gint     x_root,
                                               gint     y_root,
                                               DWORD    grfKeyState)
{
  GdkWin32Drop *drop_win32 = GDK_WIN32_DROP (drop);

  if (x_root != drop_win32->last_x ||
      y_root != drop_win32->last_y ||
      grfKeyState != drop_win32->last_key_state)
    return TRUE;

  return FALSE;
}

void
_gdk_win32_local_drop_target_dragover (GdkDrop        *drop,
                                       GdkDrag        *drag,
                                       gint            x_root,
                                       gint            y_root,
                                       DWORD           grfKeyState,
                                       guint32         time_,
                                       GdkDragAction  *actions)
{
  GdkWin32Drop *drop_win32 = GDK_WIN32_DROP (drop);
  GdkDragAction source_actions;

  source_actions = set_source_actions_helper (drop, *actions, grfKeyState);

  GDK_NOTE (DND, g_print ("_gdk_win32_local_drop_target_dragover %p @ %d : %d"
                          ", actions = %s\n",
                          drop, x_root, y_root,
                          _gdk_win32_drag_action_to_string (*actions)));

  if (_gdk_win32_local_drop_target_will_emit_motion (drop, x_root, y_root, grfKeyState))
    {
      gdk_drop_emit_motion_event (drop, TRUE, x_root, y_root, time_);
      drop_win32->last_key_state = grfKeyState;
      drop_win32->last_x = x_root;
      drop_win32->last_y = y_root;
    }

  *actions = filter_actions (drop_win32->actions, source_actions);

  GDK_NOTE (DND, g_print ("_gdk_win32_local_drop_target_dragover returns with actions %s\n",
                          _gdk_win32_drag_action_to_string (*actions)));
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
                      LPDWORD      pdwEffect_and_dwOKEffects)
{
  drop_target_context *ctx = (drop_target_context *) This;
  GdkWin32Drop *drop_win32 = GDK_WIN32_DROP (ctx->drop);
  gint pt_x = pt.x / drop_win32->scale + _gdk_offset_x;
  gint pt_y = pt.y / drop_win32->scale + _gdk_offset_y;
  GdkDragAction source_actions;
  GdkDragAction dest_actions;

  source_actions = set_source_actions_helper (ctx->drop,
                                              actions_for_drop_effects (*pdwEffect_and_dwOKEffects),
                                              grfKeyState);

  GDK_NOTE (DND, g_print ("idroptarget_dragover %p @ %d : %d"
                          " (raw %ld : %ld)"
                          ", dwOKEffects = %lu"
                          ", suggests %d action\n",
                          This, pt_x, pt_y,
                          pt.x, pt.y,
                          *pdwEffect_and_dwOKEffects,
                          source_actions));

  if (pt_x != drop_win32->last_x ||
      pt_y != drop_win32->last_y ||
      grfKeyState != drop_win32->last_key_state)
    {
      gdk_drop_emit_motion_event (ctx->drop, TRUE, pt_x, pt_y, GDK_CURRENT_TIME);
      drop_win32->last_key_state = grfKeyState;
      drop_win32->last_x = pt_x;
      drop_win32->last_y = pt_y;
    }

  dest_actions = filter_actions (drop_win32->actions, source_actions);
  *pdwEffect_and_dwOKEffects = drop_effect_for_actions (dest_actions);

  GDK_NOTE (DND, g_print ("idroptarget_dragover returns S_OK with actions %s"
                          " and effect %lu\n",
                          _gdk_win32_drag_action_to_string (dest_actions),
                          *pdwEffect_and_dwOKEffects));

  return S_OK;
}

void
_gdk_win32_local_drop_target_dragleave (GdkDrop *drop,
                                        guint32  time_)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (gdk_drop_get_surface (drop));
  GDK_NOTE (DND, g_print ("_gdk_win32_local_drop_target_dragleave %p\n", drop));

  gdk_drop_emit_leave_event (drop, TRUE, time_);

  g_clear_object (&impl->drop);
}

static HRESULT STDMETHODCALLTYPE
idroptarget_dragleave (LPDROPTARGET This)
{
  drop_target_context *ctx = (drop_target_context *) This;

  GDK_NOTE (DND, g_print ("idroptarget_dragleave %p S_OK\n", This));

  gdk_drop_emit_leave_event (GDK_DROP (ctx->drop), TRUE, GDK_CURRENT_TIME);

  g_clear_object (&ctx->drop);
  set_data_object (&ctx->data_object, NULL);

  return S_OK;
}

void
_gdk_win32_local_drop_target_drop (GdkDrop        *drop,
                                   GdkDrag        *drag,
                                   guint32         time_,
                                   GdkDragAction  *actions)
{
  GdkWin32Drop *drop_win32 = GDK_WIN32_DROP (drop);

  GDK_NOTE (DND, g_print ("_gdk_win32_local_drop_target_drop %p ", drop));

  set_source_actions_helper (drop,
                             *actions,
                             drop_win32->last_key_state);

  drop_win32->drop_finished = FALSE;
  gdk_drop_emit_drop_event (drop, TRUE, drop_win32->last_x, drop_win32->last_y, time_);

  while (!drop_win32->drop_finished)
    g_main_context_iteration (NULL, FALSE);

  /* Notify local source of the DnD result
   * Special case:
   * drop_win32->actions is guaranteed to contain 1 action after gdk_drop_finish ()
   */
  *actions = drop_win32->actions;

  GDK_NOTE (DND, g_print ("drop with action %s\n", _gdk_win32_drag_action_to_string (*actions)));
}

static HRESULT STDMETHODCALLTYPE
idroptarget_drop (LPDROPTARGET This,
                  LPDATAOBJECT pDataObj,
                  DWORD        grfKeyState,
                  POINTL       pt,
                  LPDWORD      pdwEffect_and_dwOKEffects)
{
  drop_target_context *ctx = (drop_target_context *) This;
  GdkWin32Drop *drop_win32 = GDK_WIN32_DROP (ctx->drop);
  gint pt_x = pt.x / drop_win32->scale + _gdk_offset_x;
  gint pt_y = pt.y / drop_win32->scale + _gdk_offset_y;
  GdkDragAction dest_action;

  GDK_NOTE (DND, g_print ("idroptarget_drop %p ", This));

  if (pDataObj == NULL)
    {
      GDK_NOTE (DND, g_print ("E_POINTER\n"));
      gdk_drop_emit_leave_event (ctx->drop, TRUE, GDK_CURRENT_TIME);
      g_clear_object (&ctx->drop);
      set_data_object (&ctx->data_object, NULL);

      return E_POINTER;
    }

  set_source_actions_helper (ctx->drop,
                             actions_for_drop_effects (*pdwEffect_and_dwOKEffects),
                             grfKeyState);

  drop_win32->drop_finished = FALSE;
  gdk_drop_emit_drop_event (ctx->drop, TRUE, pt_x, pt_y, GDK_CURRENT_TIME);

  while (!drop_win32->drop_finished)
    g_main_context_iteration (NULL, FALSE);

  /* Notify OLE of the DnD result
   * Special case:
   * drop_win32->actions is guaranteed to contain 1 action after gdk_drop_finish ()
   */
  dest_action = drop_win32->actions;
  *pdwEffect_and_dwOKEffects = drop_effect_for_actions (dest_action);

  g_clear_object (&ctx->drop);
  set_data_object (&ctx->data_object, NULL);

  GDK_NOTE (DND, g_print ("drop S_OK with effect %lx\n", *pdwEffect_and_dwOKEffects));

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

static drop_target_context *
target_context_new (GdkSurface *window)
{
  drop_target_context *result;

  result = g_new0 (drop_target_context, 1);
  result->idt.lpVtbl = &idt_vtbl;
  result->ref_count = 0;

  result->surface = window;

  idroptarget_addref (&result->idt);

  GDK_NOTE (DND, g_print ("target_context_new: %p (window %p)\n", result, result->surface));

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
  GdkDrop *drop;
  GdkWin32Drop *drop_win32;
  GString *result;
  HANDLE hdrop;
  POINT pt;
  gint nfiles, i;
  gchar *fileName, *linkedFile;

  if (msg->message != WM_DROPFILES)
    return GDK_WIN32_MESSAGE_FILTER_CONTINUE;

  GDK_NOTE (DND, g_print ("WM_DROPFILES: %p\n", msg->hwnd));

  window = gdk_win32_handle_table_lookup (msg->hwnd);

  drop = gdk_drop_new (GDK_DISPLAY (display),
                       gdk_seat_get_pointer (gdk_display_get_default_seat (GDK_DISPLAY (display))),
                       NULL,
                       /* WM_DROPFILES drops are always file names */
                       gdk_content_formats_new ((const char *[2]) {
                                                  "text/uri-list",
                                                  NULL
                                                }, 1),
                       window,
                       GDK_DRAG_PROTO_WIN32_DROPFILES);
  drop_win32 = GDK_WIN32_DROP (drop);

  gdk_drop_set_actions (drop, GDK_ACTION_COPY);

  hdrop = (HANDLE) msg->wParam;
  DragQueryPoint (hdrop, &pt);
  ClientToScreen (msg->hwnd, &pt);

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

  g_clear_pointer (&drop_win32->dropfiles_list, g_free);
  drop_win32->dropfiles_list = result->str;
  g_string_free (result, FALSE);
  if (drop_win32->dropfiles_list == NULL)
    drop_win32->dropfiles_list = g_strdup ("");

  gdk_drop_emit_drop_event (drop,
                            FALSE, 
                            pt.x / drop_win32->scale + _gdk_offset_x,
                            pt.y / drop_win32->scale + _gdk_offset_y,
                            _gdk_win32_get_next_tick (msg->time));

  DragFinish (hdrop);

  *ret_valp = 0;

  return GDK_WIN32_MESSAGE_FILTER_REMOVE;
}

static void
gdk_win32_drop_status (GdkDrop       *drop,
                       GdkDragAction  actions)
{
  GdkWin32Drop *drop_win32 = GDK_WIN32_DROP (drop);
  GdkDrag *drag;

  g_return_if_fail (drop != NULL);

  GDK_NOTE (DND, g_print ("gdk_win32_drop_status: %s\n"
                          " context=%p:{source_actions=%s}\n",
                          _gdk_win32_drag_action_to_string (actions),
                          drop,
                          _gdk_win32_drag_action_to_string (gdk_drop_get_actions (drop))));

  drop_win32->actions = actions;

  if (drop_win32->protocol == GDK_DRAG_PROTO_OLE2)
    return;

  drag = gdk_drop_get_drag (drop);

  if (drag != NULL)
    _gdk_win32_local_drag_give_feedback (drag, actions);
}

static void
gdk_win32_drop_finish (GdkDrop       *drop,
                       GdkDragAction  action)
{
  GdkWin32Drop *drop_win32 = GDK_WIN32_DROP (drop);

  g_return_if_fail (drop != NULL);

  GDK_NOTE (DND, g_print ("gdk_win32_drop_finish with action %s\n",
                          _gdk_win32_drag_action_to_string (action)));

  drop_win32->actions = action;
  drop_win32->drop_finished = TRUE;

  if (drop_win32->protocol == GDK_DRAG_PROTO_OLE2)
    return;
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
  drop_target_context *ctx;
  HRESULT hr;

  g_return_if_fail (window != NULL);

  if (g_object_get_data (G_OBJECT (window), "gdk-dnd-registered") != NULL)
    return;
  else
    g_object_set_data (G_OBJECT (window), "gdk-dnd-registered", GINT_TO_POINTER (TRUE));

  GDK_NOTE (DND, g_print ("gdk_win32_surface_register_dnd: %p\n", GDK_SURFACE_HWND (window)));

  if (!use_ole2_dnd)
    {
      /* We always claim to accept dropped files, but in fact we might not,
       * of course. This function is called in such a way that it cannot know
       * whether the window (widget) in question actually accepts files
       * (in gtk, data of type text/uri-list) or not.
       */
      gdk_win32_display_add_filter (GDK_WIN32_DISPLAY (gdk_display_get_default ()), gdk_dropfiles_filter, NULL);
      DragAcceptFiles (GDK_SURFACE_HWND (window), TRUE);
    }
  else
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

      /* Return if window is already setup for DND. */
      if (impl->drop_target != NULL)
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
              impl->drop_target = ctx;
            }
        }
    }
}

void
_gdk_win32_surface_unregister_dnd (GdkSurface *window)
{
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (window);

  if (impl->drop_target)
    idroptarget_release (&impl->drop_target->idt);
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
gdk_win32_drop_read_async (GdkDrop             *drop,
                           GdkContentFormats   *formats,
                           int                  io_priority,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GdkWin32Drop              *drop_win32 = GDK_WIN32_DROP (drop);
  GTask                     *task;
  drop_target_context       *tctx;
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
  g_task_set_source_tag (task, gdk_win32_drop_read_async);

  mime_types = gdk_content_formats_get_mime_types (formats, &n_mime_types);

  if (drop_win32->protocol == GDK_DRAG_PROTO_WIN32_DROPFILES)
    {
      for (i = 0; i < n_mime_types; i++)
        if (g_strcmp0 (mime_types[i], "text/uri-list") == 0)
          break;
      if (i >= n_mime_types)
        {
          g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                   _("No compatible transfer format found"));
          g_clear_pointer (&drop_win32->dropfiles_list, g_free);

          return;
        }

      stream = g_memory_input_stream_new_from_data (drop_win32->dropfiles_list, strlen (drop_win32->dropfiles_list), g_free);
      drop_win32->dropfiles_list = NULL;
      g_object_set_data (G_OBJECT (stream), "gdk-dnd-stream-contenttype", (gpointer) "text/uri-list");
      g_task_return_pointer (task, stream, g_object_unref);

      return;
    }

  tctx = GDK_WIN32_SURFACE (gdk_drop_get_surface (drop))->drop_target;

  if (tctx == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("GDK surface 0x%p is not registered as a drop target"), gdk_drop_get_surface (drop));
      return;
    }

  if (tctx->data_object == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Target context record 0x%p has no data object"), tctx);
      return;
    }

  for (pair = NULL, i = 0; i < n_mime_types; i++)
    {
      for (j = 0; j < drop_win32->droptarget_w32format_contentformat_map->len; j++)
        {
          pair = &g_array_index (drop_win32->droptarget_w32format_contentformat_map, GdkWin32ContentFormatPair, j);
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
gdk_win32_drop_read_finish (GdkDrop       *drop,
                            GAsyncResult  *result,
                            const char   **out_mime_type,
                            GError       **error)
{
  GTask *task;
  GInputStream *stream;

  g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (drop)), NULL);
  task = G_TASK (result);
  g_return_val_if_fail (g_task_get_source_tag (task) == gdk_win32_drop_read_async, NULL);

  stream = g_task_propagate_pointer (task, error);

  if (stream && out_mime_type)
    *out_mime_type = g_object_get_data (G_OBJECT (stream), "gdk-dnd-stream-contenttype");

  return stream;
}

static void
gdk_win32_drop_class_init (GdkWin32DropClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDropClass *drop_class = GDK_DROP_CLASS (klass);

  object_class->finalize = gdk_win32_drop_finalize;

  drop_class->status = gdk_win32_drop_status;
  drop_class->finish = gdk_win32_drop_finish;
  drop_class->read_async = gdk_win32_drop_read_async;
  drop_class->read_finish = gdk_win32_drop_read_finish;
}

void
_gdk_drop_init (void)
{
  if (g_strcmp0 (getenv ("GDK_WIN32_OLE2_DND"), "0") == 0)
    use_ole2_dnd = FALSE;
}
