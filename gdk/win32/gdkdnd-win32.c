/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-1999 Tor Lillqvist
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

#define INITGUID

#include <string.h>
#include "gdk.h"
#include "gdkx.h"
#include "gdk/gdkprivate.h"
#include <ole2.h>
#include <shlobj.h>
#include <shlguid.h>

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

HRESULT STDMETHODCALLTYPE
  m_query_interface_target (IDropTarget __RPC_FAR *This,
			    /* [in] */ REFIID riid,
			    /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);

ULONG STDMETHODCALLTYPE
  m_add_ref_target (IDropTarget __RPC_FAR *This);

ULONG STDMETHODCALLTYPE
  m_release_target (IDropTarget __RPC_FAR *This);

HRESULT STDMETHODCALLTYPE 
  m_drag_enter (IDropTarget __RPC_FAR *This,
		/* [unique][in] */ IDataObject __RPC_FAR *pDataObj,
		/* [in] */ DWORD grfKeyState,
		/* [in] */ POINTL pt,
		/* [out][in] */ DWORD __RPC_FAR *pdwEffect);

HRESULT STDMETHODCALLTYPE
  m_drag_over (IDropTarget __RPC_FAR *This,
	       /* [in] */ DWORD grfKeyState,
	       /* [in] */ POINTL pt,
	       /* [out][in] */ DWORD __RPC_FAR *pdwEffect);

HRESULT STDMETHODCALLTYPE
  m_drag_leave (IDropTarget __RPC_FAR *This);

HRESULT STDMETHODCALLTYPE
  m_drop (IDropTarget __RPC_FAR *This,
	  /* [unique][in] */ IDataObject __RPC_FAR *pDataObj,
	  /* [in] */ DWORD grfKeyState,
	  /* [in] */ POINTL pt,
	  /* [out][in] */ DWORD __RPC_FAR *pdwEffect);

HRESULT STDMETHODCALLTYPE
  m_query_interface_source (IDropSource __RPC_FAR *This,
			    /* [in] */ REFIID riid,
			    /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);

ULONG STDMETHODCALLTYPE
  m_add_ref_source (IDropSource __RPC_FAR *This);

ULONG STDMETHODCALLTYPE
  m_release_source (IDropSource __RPC_FAR *This);

HRESULT STDMETHODCALLTYPE
  m_query_continue_drag (IDropSource __RPC_FAR *This,
			 /* [in] */ BOOL fEscapePressed,
			 /* [in] */ DWORD grfKeyState);
HRESULT STDMETHODCALLTYPE
  m_give_feedback (IDropSource __RPC_FAR *This,
		   /* [in] */ DWORD dwEffect);

#endif /* OLE2_DND */

/* Structure that holds information about a drag in progress.
 * this is used on both source and destination sides.
 */
struct _GdkDragContextPrivate {
  GdkDragContext context;

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

#if OLE2_DND

typedef struct {
  IDropTarget idt;
  GdkDragContext *context;
} target_drag_context;

typedef struct {
  IDropSource ids;
  GdkDragContext *context;
} source_drag_context;

HRESULT STDMETHODCALLTYPE
m_query_interface_target (IDropTarget __RPC_FAR *This,
			  /* [in] */ REFIID riid,
			  /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject)
{
  GDK_NOTE (DND, g_print ("m_query_interface_target\n"));

  *ppvObject = NULL;

  g_print ("riid = %.08x-%.04x-%.04x-%.02x%.02x-%.02x%.02x%.02x%.02x%.02x%.02x",
	   ((gulong *) riid)[0],
	   ((gushort *) riid)[2],
	   ((gushort *) riid)[3],
	   ((guchar *) riid)[8],
	   ((guchar *) riid)[9],
	   ((guchar *) riid)[10],
	   ((guchar *) riid)[11],
	   ((guchar *) riid)[12],
	   ((guchar *) riid)[13],
	   ((guchar *) riid)[14],
	   ((guchar *) riid)[15]);
  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      m_add_ref_target (This);
      *ppvObject = This;
      g_print ("...IUnknown\n");
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDropTarget))
    {
      m_add_ref_target (This);
      *ppvObject = This;
      g_print ("...IDropTarget\n");
      return S_OK;
    }
  else
    {
      g_print ("...Huh?\n");
      return E_NOINTERFACE;
    }
}

ULONG STDMETHODCALLTYPE
m_add_ref_target (IDropTarget __RPC_FAR *This)
{
  target_drag_context *ctx = (target_drag_context *) This;
  GdkDragContextPrivate *private = (GdkDragContextPrivate *) ctx->context;

  GDK_NOTE (DND, g_print ("m_add_ref_target\n"));
  gdk_drag_context_ref (ctx->context);
  
  return private->ref_count;
}

ULONG STDMETHODCALLTYPE
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

HRESULT STDMETHODCALLTYPE 
m_drag_enter (IDropTarget __RPC_FAR *This,
	      /* [unique][in] */ IDataObject __RPC_FAR *pDataObj,
	      /* [in] */ DWORD grfKeyState,
	      /* [in] */ POINTL pt,
	      /* [out][in] */ DWORD __RPC_FAR *pdwEffect)
{
  GDK_NOTE (DND, g_print ("m_drag_enter\n"));
  return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE
m_drag_over (IDropTarget __RPC_FAR *This,
	     /* [in] */ DWORD grfKeyState,
	     /* [in] */ POINTL pt,
	     /* [out][in] */ DWORD __RPC_FAR *pdwEffect)
{
  GDK_NOTE (DND, g_print ("m_drag_over\n"));
  return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE
m_drag_leave (IDropTarget __RPC_FAR *This)
{
  GDK_NOTE (DND, g_print ("m_drag_leave\n"));
  return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE
m_drop (IDropTarget __RPC_FAR *This,
	/* [unique][in] */ IDataObject __RPC_FAR *pDataObj,
	/* [in] */ DWORD grfKeyState,
	/* [in] */ POINTL pt,
	/* [out][in] */ DWORD __RPC_FAR *pdwEffect)
{
  GDK_NOTE (DND, g_print ("m_drop\n"));
  return E_UNEXPECTED;
}
 
HRESULT STDMETHODCALLTYPE
m_query_interface_source (IDropSource __RPC_FAR *This,
			  /* [in] */ REFIID riid,
			  /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject)
{
  GDK_NOTE (DND, g_print ("m_query_interface_source\n"));

  *ppvObject = NULL;

  g_print ("riid = %.02x%.02x%.02x%.02x-%.02x%.02x-%.02x%.02x-%.02x%.02x-%.02x%.02x%.02x%.02x%.02x%.02x",
	   ((guchar *) riid)[0],
	   ((guchar *) riid)[1],
	   ((guchar *) riid)[2],
	   ((guchar *) riid)[3],
	   ((guchar *) riid)[4],
	   ((guchar *) riid)[5],
	   ((guchar *) riid)[6],
	   ((guchar *) riid)[7],
	   ((guchar *) riid)[8],
	   ((guchar *) riid)[9],
	   ((guchar *) riid)[10],
	   ((guchar *) riid)[11],
	   ((guchar *) riid)[12],
	   ((guchar *) riid)[13],
	   ((guchar *) riid)[14],
	   ((guchar *) riid)[15]);
  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      m_add_ref_source (This);
      *ppvObject = This;
      g_print ("...IUnknown\n");
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDropSource))
    {
      m_add_ref_source (This);
      *ppvObject = This;
      g_print ("...IDropSource\n");
      return S_OK;
    }
  else
    {
      g_print ("...Huh?\n");
      return E_NOINTERFACE;
    }
}

ULONG STDMETHODCALLTYPE
m_add_ref_source (IDropSource __RPC_FAR *This)
{
  source_drag_context *ctx = (source_drag_context *) This;
  GdkDragContextPrivate *private = (GdkDragContextPrivate *) ctx->context;

  GDK_NOTE (DND, g_print ("m_add_ref_source\n"));
  gdk_drag_context_ref (ctx->context);
  
  return private->ref_count;
}

ULONG STDMETHODCALLTYPE
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

HRESULT STDMETHODCALLTYPE
m_query_continue_drag (IDropSource __RPC_FAR *This,
		       /* [in] */ BOOL fEscapePressed,
		       /* [in] */ DWORD grfKeyState)
{
  GDK_NOTE (DND, g_print ("m_query_continue_drag\n"));
  return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE
m_give_feedback (IDropSource __RPC_FAR *This,
		 /* [in] */ DWORD dwEffect)
{
  GDK_NOTE (DND, g_print ("m_give_feedback\n"));
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

target_drag_context *
target_context_new (void)
{
  target_drag_context *result;

  result = g_new0 (target_drag_context, 1);

  result->idt.lpVtbl = &idt_vtbl;

  result->context = gdk_drag_context_new ();

  return result;
}

source_drag_context *
source_context_new (void)
{
  source_drag_context *result;

  result = g_new0 (source_drag_context, 1);

  result->ids.lpVtbl = &ids_vtbl;

  result->context = gdk_drag_context_new ();

  return result;
}

#endif /* OLE2_DND */

void            
gdk_drag_context_ref (GdkDragContext *context)
{
  g_return_if_fail (context != NULL);

  ((GdkDragContextPrivate *)context)->ref_count++;
}

void            
gdk_drag_context_unref (GdkDragContext *context)
{
  GdkDragContextPrivate *private = (GdkDragContextPrivate *)context;
  
  g_return_if_fail (context != NULL);

  private->ref_count--;

  GDK_NOTE (DND, g_print ("gdk_drag_context_unref: %d%s\n",
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
      g_free (private);
    }
}

#if 0

static GdkDragContext *
gdk_drag_context_find (gboolean is_source,
		       HWND     source_xid,
		       HWND     dest_xid)
{
  GList *tmp_list = contexts;
  GdkDragContext *context;

  while (tmp_list)
    {
      context = (GdkDragContext *)tmp_list->data;

      if ((!context->is_source == !is_source) &&
	  ((source_xid == None) || (context->source_window &&
	    (GDK_WINDOW_XWINDOW (context->source_window) == source_xid))) &&
	  ((dest_xid == None) || (context->dest_window &&
	    (GDK_WINDOW_XWINDOW (context->dest_window) == dest_xid))))
	return context;
      
      tmp_list = tmp_list->next;
    }

  return NULL;
}

#endif

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
  static GdkAtom text_uri_list_atom = GDK_NONE;
  GString *result;
  MSG *msg = (MSG *) xev;
  HANDLE hdrop;
  POINT pt;
  gint nfiles, i, k;
  guchar fileName[MAX_PATH], linkedFile[MAX_PATH];
  
  if (text_uri_list_atom == GDK_NONE)
    text_uri_list_atom = gdk_atom_intern ("text/uri-list", FALSE);

  if (msg->message == WM_DROPFILES)
    {
      GDK_NOTE (DND, g_print ("WM_DROPFILES: %#x\n", msg->hwnd));

      context = gdk_drag_context_new ();
      private = (GdkDragContextPrivate *) context;
      context->protocol = GDK_DRAG_PROTO_WIN32_DROPFILES;
      context->is_source = FALSE;
      context->source_window = (GdkWindow *) &gdk_root_parent;
      context->dest_window = event->any.window;
      gdk_window_ref (context->dest_window);
      /* WM_DROPFILES drops are always file names */
      context->targets =
	g_list_append (NULL, GUINT_TO_POINTER (text_uri_list_atom));
      current_dest_drag = context;

      event->dnd.type = GDK_DROP_START;
      event->dnd.context = current_dest_drag;
      gdk_drag_context_ref (current_dest_drag);
      
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
	  g_string_append (result, "file:");
	  DragQueryFile (hdrop, i, fileName, MAX_PATH);

	  /* Resolve shortcuts */
	  if (resolve_link (msg->hwnd, fileName, linkedFile, NULL))
	    {
	      g_string_append (result, linkedFile);
	      GDK_NOTE (DND, g_print ("...%s link to %s\n",
				      fileName, linkedFile));
	    }
	  else
	    {
	      g_string_append (result, fileName);
	      GDK_NOTE (DND, g_print ("...%s\n", fileName));
	    }
	  g_string_append (result, "\015\012");
	}
      gdk_sel_prop_store ((GdkWindow *) &gdk_root_parent,
			  text_uri_list_atom, 8, result->str, result->len + 1);

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
  HRESULT hres;

  hres = OleInitialize (NULL);

  if (! SUCCEEDED (hres))
    g_error ("OleInitialize failed");
}		      

void
gdk_dnd_exit (void)
{
  OleUninitialize ();
}

/* Source side */

static void
gdk_drag_do_leave (GdkDragContext *context, guint32 time)
{
  if (context->dest_window)
    {
      GDK_NOTE (DND, g_print ("gdk_drag_do_leave\n"));
      gdk_window_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

GdkDragContext * 
gdk_drag_begin (GdkWindow     *window,
		GList         *targets)
{
  GList *tmp_list;
  GdkDragContext *new_context;
  
  g_return_val_if_fail (window != NULL, NULL);

  GDK_NOTE (DND, g_print ("gdk_drag_begin\n"));

  new_context = gdk_drag_context_new ();
  new_context->is_source = TRUE;
  new_context->source_window = window;
  gdk_window_ref (window);

  tmp_list = g_list_last (targets);
  new_context->targets = NULL;
  while (tmp_list)
    {
      new_context->targets = g_list_prepend (new_context->targets,
					     tmp_list->data);
      tmp_list = tmp_list->prev;
    }

  new_context->actions = 0;

  return new_context;
}

guint32
gdk_drag_get_protocol (guint32          xid,
		       GdkDragProtocol *protocol)
{
  /* This isn't used */
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
  GdkDragContextPrivate *private = (GdkDragContextPrivate *)context;
  GdkWindowPrivate *drag_window_private = (GdkWindowPrivate *) drag_window;
  HWND recipient;
  POINT pt;

  GDK_NOTE (DND, g_print ("gdk_drag_find_window: %#x +%d+%d\n",
			  (drag_window ? drag_window_private->xwindow : 0),
			  x_root, y_root));

  pt.x = x_root;
  pt.y = y_root;
  recipient = WindowFromPoint (pt);
  if (recipient == NULL)
    *dest_window = NULL;
  else
    {
      *dest_window = gdk_window_lookup (recipient);
      if (*dest_window)
	gdk_window_ref (*dest_window);
      *protocol = GDK_DRAG_PROTO_WIN32_DROPFILES;
    }
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
  return FALSE;
}

void
gdk_drag_drop (GdkDragContext *context,
	       guint32         time)
{
  g_return_if_fail (context != NULL);

  g_warning ("gdk_drag_drop: not implemented\n");
}

void
gdk_drag_abort (GdkDragContext *context,
		guint32         time)
{
  g_return_if_fail (context != NULL);

  gdk_drag_do_leave (context, time);
}

/* Destination side */

void             
gdk_drag_status (GdkDragContext   *context,
		 GdkDragAction     action,
		 guint32           time)
{
  GDK_NOTE (DND, g_print ("gdk_drag_status\n"));
}

void 
gdk_drop_reply (GdkDragContext   *context,
		gboolean          ok,
		guint32           time)
{
}

void             
gdk_drop_finish (GdkDragContext   *context,
		 gboolean          success,
		 guint32           time)
{
}


void            
gdk_window_register_dnd (GdkWindow      *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *) window;
#if OLE2_DND
  target_drag_context *context;
  HRESULT hres;
#endif

  g_return_if_fail (window != NULL);

  GDK_NOTE (DND, g_print ("gdk_window_register_dnd: %#x\n", private->xwindow));

  /* We always claim to accept dropped files, but in fact we might not,
   * of course. This function is called in such a way that it cannot know
   * whether the window (widget) in question actually accepts files
   * (in gtk, data of type text/uri-list) or not.
   */
  gdk_window_add_filter (window, gdk_dropfiles_filter, NULL);
  DragAcceptFiles (private->xwindow, TRUE);

#if OLE2_DND
  /* Register for OLE2 d&d */
  context = target_context_new ();
  hres = CoLockObjectExternal ((IUnknown *) &context->idt, TRUE, FALSE);
  if (!SUCCEEDED (hres))
    g_warning ("gdk_window_register_dnd: CoLockObjectExternal failed");
  else
    {
      hres = RegisterDragDrop (private->xwindow, &context->idt);
      if (hres == DRAGDROP_E_ALREADYREGISTERED)
	{
	  g_print ("DRAGDROP_E_ALREADYREGISTERED\n");
	  CoLockObjectExternal ((IUnknown *) &context->idt, FALSE, FALSE);
	}
      else if (!SUCCEEDED (hres))
	g_warning ("gdk_window_register_dnd: RegisterDragDrop failed");
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
  if (context->protocol == GDK_DRAG_PROTO_WIN32_DROPFILES)
    return gdk_win32_dropfiles_atom;
  else if (context->protocol == GDK_DRAG_PROTO_OLE2)
    return gdk_ole2_dnd_atom;
  else
    return GDK_NONE;
}
