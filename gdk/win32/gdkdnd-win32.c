/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <string.h>

/* #define OLE2_DND */

#define INITGUID

#include "gdkdnd.h"
#include "gdkproperty.h"
#include "gdkprivate.h"
#include "gdkwin32.h"

#ifdef OLE2_DND
#include <ole2.h>
#else
#include <objbase.h>
#endif

#include <shlobj.h>
#include <shlguid.h>

#include <gdk/gdk.h>

typedef struct _GdkDragContextPrivate GdkDragContextPrivate;

typedef enum {
  GDK_DRAG_STATUS_DRAG,
  GDK_DRAG_STATUS_MOTION_WAIT,
  GDK_DRAG_STATUS_ACTION_WAIT,
  GDK_DRAG_STATUS_DROP
} GtkDragStatus;

typedef enum {
  GDK_DRAG_SOURCE,
  GDK_DRAG_TARGET
} GdkDragKind;

#ifdef OLE2_DND

#define PRINT_GUID(guid) \
  g_print ("guid = %.08x-%.04x-%.04x-%.02x%.02x-%.02x%.02x%.02x%.02x%.02x%.02x", \
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


#endif /* OLE2_DND */

/* Structure that holds information about a drag in progress.
 * this is used on both source and destination sides.
 */
struct _GdkDragContextPrivate {
  GdkDragContext context;

  GdkAtom local_selection;
  guint   ref_count;

  guint16 last_x;		/* Coordinates from last event */
  guint16 last_y;
  HWND  dest_xid;
  guint drag_status;		/* Current status of drag */
};

GdkDragContext *current_dest_drag = NULL;

/* Drag Contexts */

static GList *contexts;

GdkDragContext *
gdk_drag_context_new        (void)
{
  GdkDragContextPrivate *result;

  result = g_new0 (GdkDragContextPrivate, 1);

  result->ref_count = 1;

  contexts = g_list_prepend (contexts, result);

  return (GdkDragContext *)result;
}

void
gdk_drag_context_ref (GdkDragContext *context)
{
  GdkDragContextPrivate *private = (GdkDragContextPrivate *)context;

  g_return_if_fail (context != NULL);

  private->ref_count++;

  GDK_NOTE (DND, g_print ("gdk_drag_context_ref: %p %d\n", private, private->ref_count));
}

void
gdk_drag_context_unref (GdkDragContext *context)
{
  GdkDragContextPrivate *private = (GdkDragContextPrivate *)context;
  
  g_return_if_fail (context != NULL);

  private->ref_count--;

  GDK_NOTE (DND, g_print ("gdk_drag_context_unref: %p %d%s\n",
			  private,
			  private->ref_count,
			  (private->ref_count == 0 ? " freeing" : "")));

  if (private->ref_count == 0)
    {
      g_dataset_destroy (private);
      
      g_list_free (context->targets);

      if (context->source_window)
	gdk_window_unref (context->source_window);

      if (context->dest_window)
	gdk_window_unref (context->dest_window);

      contexts = g_list_remove (contexts, private);

      if (context == current_dest_drag)
	current_dest_drag = NULL;

      g_free (private);
    }
}

static GdkDragContext *
gdk_drag_context_find (gboolean  is_source,
		       GdkWindow *source,
		       GdkWindow *dest)
{
  GList *tmp_list = contexts;
  GdkDragContext *context;

  while (tmp_list)
    {
      context = (GdkDragContext *)tmp_list->data;

      if ((!context->is_source == !is_source) &&
	  ((source == NULL) || (context->source_window && (context->source_window == source))) &&
	  ((dest == NULL) || (context->dest_window && (context->dest_window == dest))))
	return context;
      
      tmp_list = tmp_list->next;
    }

  return NULL;
}

typedef struct {
#ifdef OLE2_DND
  IDropTarget idt;
#endif
  GdkDragContext *context;
} target_drag_context;

typedef struct {
#ifdef OLE2_DND
  IDropSource ids;
#endif
  GdkDragContext *context;
} source_drag_context;

#ifdef OLE2_DND

static ULONG STDMETHODCALLTYPE
m_add_ref_target (IDropTarget __RPC_FAR *This)
{
  target_drag_context *ctx = (target_drag_context *) This;
  GdkDragContextPrivate *private = (GdkDragContextPrivate *) ctx->context;

  GDK_NOTE (DND, g_print ("m_add_ref_target\n"));
  gdk_drag_context_ref (ctx->context);
  
  return private->ref_count;
}

static HRESULT STDMETHODCALLTYPE
m_query_interface_target (IDropTarget __RPC_FAR *This,
			  REFIID riid,
			  void __RPC_FAR *__RPC_FAR *ppvObject)
{
  GDK_NOTE (DND, g_print ("m_query_interface_target\n"));

  *ppvObject = NULL;

  PRINT_GUID (riid);

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      g_print ("...IUnknown\n");
      m_add_ref_target (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDropTarget))
    {
      g_print ("...IDropTarget\n");
      m_add_ref_target (This);
      *ppvObject = This;
      return S_OK;
    }
  else
    {
      g_print ("...Huh?\n");
      return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE
m_release_target (IDropTarget __RPC_FAR *This)
{
  target_drag_context *ctx = (target_drag_context *) This;
  GdkDragContextPrivate *private = (GdkDragContextPrivate *) ctx->context;

  GDK_NOTE (DND, g_print ("m_release_target\n"));
  gdk_drag_context_unref (ctx->context);

  if (private->ref_count == 1)
    {
      gdk_drag_context_unref (ctx->context);
      return 0;
    }
  else
    return private->ref_count - 1;
}

static HRESULT STDMETHODCALLTYPE 
m_drag_enter (IDropTarget __RPC_FAR *This,
	      IDataObject __RPC_FAR *pDataObj,
	      DWORD grfKeyState,
	      POINTL pt,
	      DWORD __RPC_FAR *pdwEffect)
{
  GDK_NOTE (DND, g_print ("m_drag_enter\n"));
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
m_drag_over (IDropTarget __RPC_FAR *This,
	     DWORD grfKeyState,
	     POINTL pt,
	     DWORD __RPC_FAR *pdwEffect)
{
  GDK_NOTE (DND, g_print ("m_drag_over\n"));
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
m_drag_leave (IDropTarget __RPC_FAR *This)
{
  GDK_NOTE (DND, g_print ("m_drag_leave\n"));
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
m_drop (IDropTarget __RPC_FAR *This,
	IDataObject __RPC_FAR *pDataObj,
	DWORD grfKeyState,
	POINTL pt,
	DWORD __RPC_FAR *pdwEffect)
{
  GDK_NOTE (DND, g_print ("m_drop\n"));
  return E_UNEXPECTED;
}

static ULONG STDMETHODCALLTYPE
m_add_ref_source (IDropSource __RPC_FAR *This)
{
  source_drag_context *ctx = (source_drag_context *) This;
  GdkDragContextPrivate *private = (GdkDragContextPrivate *) ctx->context;

  GDK_NOTE (DND, g_print ("m_add_ref_source\n"));
  gdk_drag_context_ref (ctx->context);
  
  return private->ref_count;
}

static HRESULT STDMETHODCALLTYPE
m_query_interface_source (IDropSource __RPC_FAR *This,
			  REFIID riid,
			  void __RPC_FAR *__RPC_FAR *ppvObject)
{
  GDK_NOTE (DND, g_print ("m_query_interface_source\n"));

  *ppvObject = NULL;

  PRINT_GUID (riid);
  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      g_print ("...IUnknown\n");
      m_add_ref_source (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDropSource))
    {
      g_print ("...IDropSource\n");
      m_add_ref_source (This);
      *ppvObject = This;
      return S_OK;
    }
  else
    {
      g_print ("...Huh?\n");
      return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE
m_release_source (IDropSource __RPC_FAR *This)
{
  source_drag_context *ctx = (source_drag_context *) This;
  GdkDragContextPrivate *private = (GdkDragContextPrivate *) ctx->context;

  GDK_NOTE (DND, g_print ("m_release_source\n"));
  gdk_drag_context_unref (ctx->context);

  if (private->ref_count == 1)
    {
      gdk_drag_context_unref (ctx->context);
      return 0;
    }
  else
    return private->ref_count - 1;
}

static HRESULT STDMETHODCALLTYPE
m_query_continue_drag (IDropSource __RPC_FAR *This,
		       BOOL fEscapePressed,
		       DWORD grfKeyState)
{
  GDK_NOTE (DND, g_print ("m_query_continue_drag\n"));
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
m_give_feedback (IDropSource __RPC_FAR *This,
		 DWORD dwEffect)
{
  GDK_NOTE (DND, g_print ("m_give_feedback\n"));
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
m_query_interface_object (IDataObject __RPC_FAR *This,
			  REFIID riid,
			  void __RPC_FAR *__RPC_FAR *ppvObject)
{
  return E_UNEXPECTED;
}

static ULONG STDMETHODCALLTYPE
m_add_ref_object (IDataObject __RPC_FAR *This)
{
  return E_UNEXPECTED;
}

static ULONG STDMETHODCALLTYPE
m_release_object (IDataObject __RPC_FAR *This)
{
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
m_get_data (IDataObject __RPC_FAR *This,
	    FORMATETC *pFormatEtc,
	    STGMEDIUM *pMedium)
{
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
m_get_data_here (IDataObject __RPC_FAR *This,
		 FORMATETC *pFormatEtc,
		 STGMEDIUM *pMedium)
{
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
m_query_get_data (IDataObject __RPC_FAR *This,
		  FORMATETC *pFormatEtc)
{
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
m_get_canonical_format_etc (IDataObject __RPC_FAR *This,
			    FORMATETC *pFormatEtcIn,
			    FORMATETC *pFormatEtcOut)
{
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
m_set_data (IDataObject __RPC_FAR *This,
	    FORMATETC *pFormatEtc,
	    STGMEDIUM *pMedium,
	    BOOL fRelease)
{
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
m_enum_format_etc (IDataObject __RPC_FAR *This,
		   DWORD dwDirection,
		   IEnumFORMATETC **ppEnumFormatEtc)
{
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
 m_d_advise (IDataObject __RPC_FAR *This,
	     FORMATETC *pFormatetc,
	     DWORD advf,
	     IAdviseSink *pAdvSink,
	     DWORD *pdwConnection)
{
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
 m_d_unadvise (IDataObject __RPC_FAR *This,
	       DWORD dwConnection)
{
  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
m_enum_d_advise (IDataObject __RPC_FAR *This,
		 IEnumSTATDATA **ppenumAdvise)
{
  return E_UNEXPECTED;
}
		
static IDropTargetVtbl idt_vtbl = {
  m_query_interface_target,
  m_add_ref_target,
  m_release_target,
  m_drag_enter,
  m_drag_over,
  m_drag_leave,
  m_drop
};

static IDropSourceVtbl ids_vtbl = {
  m_query_interface_source,
  m_add_ref_source,
  m_release_source,
  m_query_continue_drag,
  m_give_feedback
};

static IDataObjectVtbl ido_vtbl = {
  m_query_interface_object,
  m_add_ref_object,
  m_release_object,
  m_get_data,
  m_get_data_here,
  m_query_get_data,
  m_get_canonical_format_etc,
  m_set_data,
  m_enum_format_etc,
  m_d_advise,
  m_d_unadvise,
  m_enum_d_advise
};


static target_drag_context *
target_context_new (void)
{
  target_drag_context *result;

  result = g_new0 (target_drag_context, 1);

  result->idt.lpVtbl = &idt_vtbl;

  result->context = gdk_drag_context_new ();

  return result;
}

#endif /* OLE2_DND */

static source_drag_context *
source_context_new (void)
{
  source_drag_context *result;

  result = g_new0 (source_drag_context, 1);

#ifdef OLE2_DND
  result->ids.lpVtbl = &ids_vtbl;
#endif

  result->context = gdk_drag_context_new ();

  return result;
}

/* From MS Knowledge Base article Q130698 */

/* resolve_link() fills the filename and path buffer
 * with relevant information
 * hWnd         - calling app's window handle.
 *
 * lpszLinkName - name of the link file passed into the function.
 *
 * lpszPath     - the buffer that will receive the file pathname.
 */

static HRESULT 
resolve_link(HWND hWnd,
	     LPCTSTR lpszLinkName,
	     LPSTR lpszPath,
	     LPSTR lpszDescription)
{
  HRESULT hres;
  IShellLink *psl;
  WIN32_FIND_DATA wfd;

  /* Assume Failure to start with: */
  *lpszPath = 0;
  if (lpszDescription)
    *lpszDescription = 0;

  /* Call CoCreateInstance to obtain the IShellLink interface
   * pointer. This call fails if CoInitialize is not called, so it is
   * assumed that CoInitialize has been called.
   */

  hres = CoCreateInstance (&CLSID_ShellLink,
			   NULL,
			   CLSCTX_INPROC_SERVER,
			   &IID_IShellLink,
			   (LPVOID *)&psl);
  if (SUCCEEDED (hres))
   {
     IPersistFile *ppf;
     
     /* The IShellLink interface supports the IPersistFile
      * interface. Get an interface pointer to it.
      */
     hres = psl->lpVtbl->QueryInterface (psl,
					 &IID_IPersistFile,
					 (LPVOID *) &ppf);
     if (SUCCEEDED (hres))
       {
         WORD wsz[MAX_PATH];

	 /* Convert the given link name string to wide character string. */
         MultiByteToWideChar (CP_ACP, 0,
			      lpszLinkName,
			      -1, wsz, MAX_PATH);
	 /* Load the file. */
         hres = ppf->lpVtbl->Load (ppf, wsz, STGM_READ);
         if (SUCCEEDED (hres))
	   {
	     /* Resolve the link by calling the Resolve()
	      * interface function.
	      */
	     hres = psl->lpVtbl->Resolve(psl,  hWnd,
					 SLR_ANY_MATCH |
					 SLR_NO_UI);
	     if (SUCCEEDED (hres))
	       {
		 hres = psl->lpVtbl->GetPath (psl, lpszPath,
					      MAX_PATH,
					      (WIN32_FIND_DATA*)&wfd,
					      0);

		 if (SUCCEEDED (hres) && lpszDescription != NULL)
		   {
		     hres = psl->lpVtbl->GetDescription (psl,
							 lpszDescription,
							 MAX_PATH );

		     if (!SUCCEEDED (hres))
		       return FALSE;
		   }
	       }
	   }
         ppf->lpVtbl->Release (ppf);
       }
     psl->lpVtbl->Release (psl);
   }
  return SUCCEEDED (hres);
}

static GdkFilterReturn
gdk_dropfiles_filter (GdkXEvent *xev,
		      GdkEvent  *event,
		      gpointer   data)
{
  GdkDragContext *context;
  GdkDragContextPrivate *private;
  GString *result;
  MSG *msg = (MSG *) xev;
  HANDLE hdrop;
  POINT pt;
  gint nfiles, i;
  guchar fileName[MAX_PATH], linkedFile[MAX_PATH];
  
  if (msg->message == WM_DROPFILES)
    {
      GDK_NOTE (DND, g_print ("WM_DROPFILES: %p\n", msg->hwnd));

      context = gdk_drag_context_new ();
      private = (GdkDragContextPrivate *) context;
      context->protocol = GDK_DRAG_PROTO_WIN32_DROPFILES;
      context->is_source = FALSE;
      context->source_window = gdk_parent_root;
      gdk_window_ref (context->source_window);
      context->dest_window = event->any.window;
      gdk_window_ref (context->dest_window);
      /* WM_DROPFILES drops are always file names */
      context->targets =
	g_list_append (NULL, GUINT_TO_POINTER (text_uri_list_atom));
      current_dest_drag = context;

      event->dnd.type = GDK_DROP_START;
      event->dnd.context = current_dest_drag;
      
      hdrop = (HANDLE) msg->wParam;
      DragQueryPoint (hdrop, &pt);
      ClientToScreen (msg->hwnd, &pt);

      event->dnd.x_root = pt.x;
      event->dnd.y_root = pt.y;
      event->dnd.time = msg->time;

      nfiles = DragQueryFile (hdrop, 0xFFFFFFFF, NULL, 0);

      result = g_string_new (NULL);
      for (i = 0; i < nfiles; i++)
	{
	  gchar *uri;

	  DragQueryFile (hdrop, i, fileName, MAX_PATH);

	  /* Resolve shortcuts */
	  if (resolve_link (msg->hwnd, fileName, linkedFile, NULL))
	    {
	      uri = g_filename_to_uri (linkedFile, NULL, NULL);
	      if (uri != NULL)
		{
		  g_string_append (result, uri);
		  GDK_NOTE (DND, g_print ("...%s link to %s: %s\n",
					  fileName, linkedFile, uri));
		  g_free (uri);
		}
	    }
	  else
	    {
	      uri = g_filename_to_uri (fileName, NULL, NULL);
	      if (uri != NULL)
		{
		  g_string_append (result, uri);
		  GDK_NOTE (DND, g_print ("...%s: %s\n", fileName, uri));
		  g_free (uri);
		}
	    }
	  g_string_append (result, "\015\012");
	}

      gdk_win32_dropfiles_store (result->str);
      g_string_free (result, FALSE);

      DragFinish (hdrop);
      
      return GDK_FILTER_TRANSLATE;
    }
  else
    return GDK_FILTER_CONTINUE;
}

/*************************************************************
 ************************** Public API ***********************
 *************************************************************/

void
gdk_dnd_init (void)
{
#ifdef OLE2_DND
  HRESULT hres;
  hres = OleInitialize (NULL);

  if (! SUCCEEDED (hres))
    g_error ("OleInitialize failed");
#endif
}      

void
gdk_win32_dnd_exit (void)
{
#ifdef OLE2_DND
  OleUninitialize ();
#endif
}

/* Source side */

static void
local_send_leave (GdkDragContext  *context,
		  guint32          time)
{
  GdkEvent tmp_event;
  
  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      tmp_event.dnd.type = GDK_DRAG_LEAVE;
      tmp_event.dnd.window = context->dest_window;
      /* Pass ownership of context to the event */
      tmp_event.dnd.context = current_dest_drag;
      tmp_event.dnd.send_event = FALSE;
      tmp_event.dnd.time = GDK_CURRENT_TIME; /* FIXME? */

      current_dest_drag = NULL;
      
      gdk_event_put (&tmp_event);
    }
}

static void
local_send_enter (GdkDragContext  *context,
		  guint32          time)
{
  GdkEvent tmp_event;
  GdkDragContextPrivate *private;
  GdkDragContext *new_context;

  private = (GdkDragContextPrivate *) context;
  
  if (!private->local_selection)
    private->local_selection = gdk_atom_intern ("LocalDndSelection", FALSE);

  if (current_dest_drag != NULL)
    {
      gdk_drag_context_unref (current_dest_drag);
      current_dest_drag = NULL;
    }

  new_context = gdk_drag_context_new ();
  new_context->protocol = GDK_DRAG_PROTO_LOCAL;
  new_context->is_source = FALSE;

  new_context->source_window = context->source_window;
  gdk_window_ref (new_context->source_window);
  new_context->dest_window = context->dest_window;
  gdk_window_ref (new_context->dest_window);

  new_context->targets = g_list_copy (context->targets);

  gdk_window_set_events (new_context->source_window,
			 gdk_window_get_events (new_context->source_window) |
			 GDK_PROPERTY_CHANGE_MASK);
  new_context->actions = context->actions;

  tmp_event.dnd.type = GDK_DRAG_ENTER;
  tmp_event.dnd.window = context->dest_window;
  tmp_event.dnd.send_event = FALSE;
  tmp_event.dnd.context = new_context;

  tmp_event.dnd.time = GDK_CURRENT_TIME; /* FIXME? */
  
  current_dest_drag = new_context;
  
  ((GdkDragContextPrivate *) new_context)->local_selection = 
    private->local_selection;

  gdk_event_put (&tmp_event);
}

static void
local_send_motion (GdkDragContext  *context,
		   gint             x_root, 
		   gint             y_root,
		   GdkDragAction    action,
		   guint32          time)
{
  GdkEvent tmp_event;
  
  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      tmp_event.dnd.type = GDK_DRAG_MOTION;
      tmp_event.dnd.window = current_dest_drag->dest_window;
      tmp_event.dnd.send_event = FALSE;
      tmp_event.dnd.context = current_dest_drag;

      tmp_event.dnd.time = time;

      current_dest_drag->suggested_action = action;
      current_dest_drag->actions = current_dest_drag->suggested_action;

      tmp_event.dnd.x_root = x_root;
      tmp_event.dnd.y_root = y_root;

      ((GdkDragContextPrivate *) current_dest_drag)->last_x = x_root;
      ((GdkDragContextPrivate *) current_dest_drag)->last_y = y_root;

      ((GdkDragContextPrivate *) context)->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;
      
      gdk_event_put (&tmp_event);
    }
}

static void
local_send_drop (GdkDragContext *context, guint32 time)
{
  GdkEvent tmp_event;
  
  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      GdkDragContextPrivate *private;
      private = (GdkDragContextPrivate *) current_dest_drag;

      tmp_event.dnd.type = GDK_DROP_START;
      tmp_event.dnd.window = current_dest_drag->dest_window;
      tmp_event.dnd.send_event = FALSE;

      tmp_event.dnd.context = current_dest_drag;

      tmp_event.dnd.time = GDK_CURRENT_TIME;
      
      tmp_event.dnd.x_root = private->last_x;
      tmp_event.dnd.y_root = private->last_y;

      current_dest_drag = NULL;
      
      gdk_event_put (&tmp_event);
    }

}

static void
gdk_drag_do_leave (GdkDragContext *context,
		   guint32         time)
{
  if (context->dest_window)
    {
      GDK_NOTE (DND, g_print ("gdk_drag_do_leave\n"));
      switch (context->protocol)
	{
	case GDK_DRAG_PROTO_LOCAL:
	  local_send_leave (context, time);
	  break;

	default:
	  break;
	}
      gdk_window_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

GdkDragContext * 
gdk_drag_begin (GdkWindow *window,
		GList     *targets)
{
  GList *tmp_list;
  source_drag_context *ctx;
  
  g_return_val_if_fail (window != NULL, NULL);

  GDK_NOTE (DND, g_print ("gdk_drag_begin\n"));

  ctx = source_context_new ();
  ctx->context->is_source = TRUE;
  ctx->context->source_window = window;
  gdk_window_ref (window);

  tmp_list = g_list_last (targets);
  ctx->context->targets = NULL;
  while (tmp_list)
    {
      ctx->context->targets = g_list_prepend (ctx->context->targets,
					      tmp_list->data);
      tmp_list = tmp_list->prev;
    }

  ctx->context->actions = 0;

#if 0
  DoDragDrop (...);
#endif
  return ctx->context;
}

guint32
gdk_drag_get_protocol (guint32          xid,
		       GdkDragProtocol *protocol)
{
  /* This isn't used */
  g_assert_not_reached ();
  return 0;
}

void
gdk_drag_find_window (GdkDragContext  *context,
		      GdkWindow       *drag_window,
		      gint             x_root,
		      gint             y_root,
		      GdkWindow      **dest_window,
		      GdkDragProtocol *protocol)
{
  HWND recipient;
  POINT pt;

  pt.x = x_root;
  pt.y = y_root;
  recipient = WindowFromPoint (pt);
  if (recipient == NULL)
    *dest_window = NULL;
  else
    {
      /* It's always top-level windows that are registered for DND */
      *dest_window = gdk_window_lookup (recipient);
      if (*dest_window)
	{
	  *dest_window = gdk_window_get_toplevel (*dest_window);
	  gdk_window_ref (*dest_window);
	}

      if (context->source_window)
        *protocol = GDK_DRAG_PROTO_LOCAL;
      else
	*protocol = GDK_DRAG_PROTO_WIN32_DROPFILES;
    }
  GDK_NOTE (DND, g_print ("gdk_drag_find_window: %p +%d+%d: dest=%p proto=%d\n",
			  (drag_window ? GDK_DRAWABLE_XID (drag_window) : 0),
			  x_root, y_root,
			  *dest_window, *protocol));
}

gboolean
gdk_drag_motion (GdkDragContext *context,
		 GdkWindow      *dest_window,
		 GdkDragProtocol protocol,
		 gint            x_root, 
		 gint            y_root,
		 GdkDragAction   suggested_action,
		 GdkDragAction   possible_actions,
		 guint32         time)
{
  GdkDragContextPrivate *private;

  g_return_val_if_fail (context != NULL, FALSE);

  GDK_NOTE (DND, g_print ("gdk_drag_motion\n"));

  private = (GdkDragContextPrivate *) context;
  
  if (context->dest_window != dest_window)
    {
      GdkEvent temp_event;

      /* Send a leave to the last destination */
      gdk_drag_do_leave (context, time);
      private->drag_status = GDK_DRAG_STATUS_DRAG;

      /* Check if new destination accepts drags, and which protocol */
      if (dest_window)
	{
	  context->dest_window = dest_window;
	  gdk_window_ref (context->dest_window);
	  context->protocol = protocol;

	  switch (protocol)
	    {
	    case GDK_DRAG_PROTO_LOCAL:
	      local_send_enter (context, time);
	      break;

	    default:
	      break;
	    }
	  context->suggested_action = suggested_action;
	}
      else
	{
	  context->dest_window = NULL;
	  context->action = 0;
	}

      /* Push a status event, to let the client know that
       * the drag changed 
       */

      temp_event.dnd.type = GDK_DRAG_STATUS;
      temp_event.dnd.window = context->source_window;
      /* We use this to signal a synthetic status. Perhaps
       * we should use an extra field...
       */
      temp_event.dnd.send_event = TRUE;

      temp_event.dnd.context = context;
      temp_event.dnd.time = time;

      gdk_event_put (&temp_event);
    }
  else
    {
      context->suggested_action = suggested_action;
    }

  /* Send a drag-motion event */

  private->last_x = x_root;
  private->last_y = y_root;
      
  if (context->dest_window)
    {
      if (private->drag_status == GDK_DRAG_STATUS_DRAG)
	{
	  switch (context->protocol)
	    {
	    case GDK_DRAG_PROTO_LOCAL:
	      local_send_motion (context, x_root, y_root, suggested_action, time);
	      break;
	      
	    case GDK_DRAG_PROTO_NONE:
	      g_warning ("GDK_DRAG_PROTO_NONE is not valid in gdk_drag_motion()");
	      break;

	    default:
	      break;
	    }
	}
      else
	return TRUE;
    }

  return FALSE;
}

void
gdk_drag_drop (GdkDragContext *context,
	       guint32         time)
{
  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drag_drop\n"));

  if (context->dest_window)
    {
      switch (context->protocol)
	{
	case GDK_DRAG_PROTO_LOCAL:
	  local_send_drop (context, time);
	  break;

	case GDK_DRAG_PROTO_NONE:
	  g_warning ("GDK_DRAG_PROTO_NONE is not valid in gdk_drag_drop()");
	  break;

	default:
	  break;
	}
    }
}

void
gdk_drag_abort (GdkDragContext *context,
		guint32         time)
{
  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drag_abort\n"));

  gdk_drag_do_leave (context, time);
}

/* Destination side */

void
gdk_drag_status (GdkDragContext *context,
		 GdkDragAction   action,
		 guint32         time)
{
  GdkDragContext *src_context;
  GdkEvent tmp_event;

  g_return_if_fail (context != NULL);

  src_context = gdk_drag_context_find (TRUE,
				       context->source_window,
				       context->dest_window);

  GDK_NOTE (DND, g_print ("gdk_drag_status, src_context=%p\n", src_context));

  if (src_context)
    {
      GdkDragContextPrivate *private = (GdkDragContextPrivate *) src_context;
      
      if (private->drag_status == GDK_DRAG_STATUS_MOTION_WAIT)
	private->drag_status = GDK_DRAG_STATUS_DRAG;

      tmp_event.dnd.type = GDK_DRAG_STATUS;
      tmp_event.dnd.window = context->source_window;
      tmp_event.dnd.send_event = FALSE;
      tmp_event.dnd.context = src_context;
      tmp_event.dnd.time = GDK_CURRENT_TIME; /* FIXME? */

      if (action == GDK_ACTION_DEFAULT)
	action = 0;
      
      src_context->action = action;
      
      gdk_event_put (&tmp_event);
    }
}

void 
gdk_drop_reply (GdkDragContext *context,
		gboolean        ok,
		guint32         time)
{
  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drop_reply\n"));

  if (context->dest_window)
    {
      switch (context->protocol)
	{
	case GDK_DRAG_PROTO_WIN32_DROPFILES:
	  gdk_win32_dropfiles_store (NULL);
	  break;

	default:
	  break;
	}
    }
}

void
gdk_drop_finish (GdkDragContext *context,
		 gboolean        success,
		 guint32         time)
{
  GdkDragContextPrivate *private;
  GdkDragContext *src_context;
  GdkEvent tmp_event;
	
  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drop_finish\n"));

  private = (GdkDragContextPrivate *) context;

  src_context = gdk_drag_context_find (TRUE,
				       context->source_window,
				       context->dest_window);
  if (src_context)
    {
      tmp_event.dnd.type = GDK_DROP_FINISHED;
      tmp_event.dnd.window = src_context->source_window;
      tmp_event.dnd.send_event = FALSE;
      tmp_event.dnd.context = src_context;

      gdk_event_put (&tmp_event);
    }
}

#ifdef OLE2_DND

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
      RevokeDragDrop (msg->hwnd);
      CoLockObjectExternal (idtp, FALSE, TRUE);
    }
  return GDK_FILTER_CONTINUE;
}

#endif

void
gdk_window_register_dnd (GdkWindow *window)
{
#ifdef OLE2_DND
  target_drag_context *context;
  HRESULT hres;
#endif

  g_return_if_fail (window != NULL);

  if (GPOINTER_TO_INT (gdk_drawable_get_data (window, "gdk-dnd-registered")))
    return;

  gdk_drawable_set_data (window, "gdk-dnd-registered", GINT_TO_POINTER(TRUE), NULL);

  GDK_NOTE (DND, g_print ("gdk_window_register_dnd: %p\n",
			  GDK_DRAWABLE_XID (window)));

  /* We always claim to accept dropped files, but in fact we might not,
   * of course. This function is called in such a way that it cannot know
   * whether the window (widget) in question actually accepts files
   * (in gtk, data of type text/uri-list) or not.
   */
  gdk_window_add_filter (window, gdk_dropfiles_filter, NULL);
  DragAcceptFiles (GDK_DRAWABLE_XID (window), TRUE);

#ifdef OLE2_DND
  /* Register for OLE2 d&d */
  context = target_context_new ();
  hres = CoLockObjectExternal ((IUnknown *) &context->idt, TRUE, FALSE);
  if (!SUCCEEDED (hres))
    OTHER_API_FAILED ("CoLockObjectExternal");
  else
    {
      hres = RegisterDragDrop (GDK_DRAWABLE_XID (window), &context->idt);
      if (hres == DRAGDROP_E_ALREADYREGISTERED)
	{
	  g_print ("DRAGDROP_E_ALREADYREGISTERED\n");
	  CoLockObjectExternal ((IUnknown *) &context->idt, FALSE, FALSE);
	}
      else if (!SUCCEEDED (hres))
	OTHER_API_FAILED ("RegisterDragDrop");
      else
	{
	  gdk_window_add_filter (window, gdk_destroy_filter, &context->idt);
	}
    }
#endif
}

/*************************************************************
 * gdk_drag_get_selection:
 *     Returns the selection atom for the current source window
 *   arguments:
 *
 *   results:
 *************************************************************/

GdkAtom
gdk_drag_get_selection (GdkDragContext *context)
{
  if (context->protocol == GDK_DRAG_PROTO_LOCAL)
    return ((GdkDragContextPrivate *) context)->local_selection;
  else if (context->protocol == GDK_DRAG_PROTO_WIN32_DROPFILES)
    return gdk_win32_dropfiles_atom;
  else if (context->protocol == GDK_DRAG_PROTO_OLE2)
    return gdk_ole2_dnd_atom;
  else
    return GDK_NONE;
}
