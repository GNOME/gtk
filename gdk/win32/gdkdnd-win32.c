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

#include <string.h>

/* #define OLE2_DND */

#define INITGUID

#include "gdkdnd.h"
#include "gdkproperty.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"

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


static FORMATETC *formats;
static int nformats;

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
gdk_drag_context_new (void)
{
  GdkDragContextPrivate *result;

  result = g_new0 (GdkDragContextPrivate, 1);

  result->ref_count = 1;

  contexts = g_list_prepend (contexts, result);

  return (GdkDragContext *) result;
}

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

  if (private->ref_count <= 0)
    abort ();

  private->ref_count--;

  GDK_NOTE (DND, g_print ("gdk_drag_context_unref: %d%s\n",
			  private->ref_count,
			  (private->ref_count == 0 ? " freeing" : "")));

  if (private->ref_count == 0)
    {
      g_dataset_destroy (private);
      
      g_list_free (context->targets);

      if (context->source_window)
	gdk_drawable_unref (context->source_window);

      if (context->dest_window)
	gdk_drawable_unref (context->dest_window);

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

typedef struct {
  IDataObject ido;
  int ref_count;
} data_object;

typedef struct {
  IEnumFORMATETC ief;
  int ref_count;
  int ix;
} enum_formats;

static enum_formats *enum_formats_new (void);

static ULONG STDMETHODCALLTYPE
idroptarget_addref (LPDROPTARGET This)
{
  target_drag_context *ctx = (target_drag_context *) This;
  GdkDragContextPrivate *private = (GdkDragContextPrivate *) ctx->context;

  gdk_drag_context_ref (ctx->context);
  GDK_NOTE (DND, g_print ("idroptarget_addref %#x %d\n",
			  This, private->ref_count));
  
  return private->ref_count;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_queryinterface (LPDROPTARGET This,
			    REFIID       riid,
			    LPVOID      *ppvObject)
{
  GDK_NOTE (DND, g_print ("idroptarget_queryinterface %#x\n", This));

  *ppvObject = NULL;

  PRINT_GUID (riid);

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      g_print ("...IUnknown\n");
      idroptarget_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDropTarget))
    {
      g_print ("...IDropTarget\n");
      idroptarget_addref (This);
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
idroptarget_release (LPDROPTARGET This)
{
  target_drag_context *ctx = (target_drag_context *) This;
  GdkDragContextPrivate *private = (GdkDragContextPrivate *) ctx->context;
  int ref_count = private->ref_count - 1;

  gdk_drag_context_unref (ctx->context);
  GDK_NOTE (DND, g_print ("idroptarget_release %#x %d\n",
			  This, ref_count));

  if (ref_count == 0)
    g_free (This);

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE 
idroptarget_dragenter (LPDROPTARGET This,
		       LPDATAOBJECT pDataObj,
		       DWORD        grfKeyState,
		       POINTL       pt,
		       LPDWORD      pdwEffect)
{
  GDK_NOTE (DND, g_print ("idroptarget_dragenter %#x\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_dragover (LPDROPTARGET This,
		      DWORD        grfKeyState,
		      POINTL       pt,
		      LPDWORD      pdwEffect)
{
  GDK_NOTE (DND, g_print ("idroptarget_dragover %#x\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_dragleave (LPDROPTARGET This)
{
  GDK_NOTE (DND, g_print ("idroptarget_dragleave %#x\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_drop (LPDROPTARGET This,
		  LPDATAOBJECT pDataObj,
		  DWORD        grfKeyState,
		  POINTL       pt,
		  LPDWORD      pdwEffect)
{
  GDK_NOTE (DND, g_print ("idroptarget_drop %#x\n", This));

  return E_UNEXPECTED;
}

static ULONG STDMETHODCALLTYPE
idropsource_addref (LPDROPSOURCE This)
{
  source_drag_context *ctx = (source_drag_context *) This;
  GdkDragContextPrivate *private = (GdkDragContextPrivate *) ctx->context;

  gdk_drag_context_ref (ctx->context);
  GDK_NOTE (DND, g_print ("idropsource_addref %#x %d\n",
			  This, private->ref_count));
  
  return private->ref_count;
}

static HRESULT STDMETHODCALLTYPE
idropsource_queryinterface (LPDROPSOURCE This,
			    REFIID       riid,
			    LPVOID      *ppvObject)
{
  GDK_NOTE (DND, g_print ("idropsource_queryinterface %#x\n", This));

  *ppvObject = NULL;

  PRINT_GUID (riid);
  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      g_print ("...IUnknown\n");
      idropsource_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDropSource))
    {
      g_print ("...IDropSource\n");
      idropsource_addref (This);
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
idropsource_release (LPDROPSOURCE This)
{
  source_drag_context *ctx = (source_drag_context *) This;
  GdkDragContextPrivate *private = (GdkDragContextPrivate *) ctx->context;
  int ref_count = private->ref_count - 1;

  gdk_drag_context_unref (ctx->context);
  GDK_NOTE (DND, g_print ("idropsource_release %#x %d\n",
			  This, ref_count));

  if (ref_count == 0)
    g_free (This);

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
idropsource_querycontinuedrag (LPDROPSOURCE This,
			       BOOL         fEscapePressed,
			       DWORD        grfKeyState)
{
  GDK_NOTE (DND, g_print ("idropsource_querycontinuedrag %#x\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idropsource_givefeedback (LPDROPSOURCE This,
			  DWORD        dwEffect)
{
  GDK_NOTE (DND, g_print ("idropsource_givefeedback %#x\n", This));

  return E_UNEXPECTED;
}

static ULONG STDMETHODCALLTYPE
idataobject_addref (LPDATAOBJECT This)
{
  data_object *dobj = (data_object *) This;
  int ref_count = ++dobj->ref_count;

  GDK_NOTE (DND, g_print ("idataobject_addref %#x %d\n", This, ref_count));

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
idataobject_queryinterface (LPDATAOBJECT This,
			    REFIID       riid,
			    LPVOID      *ppvObject)
{
  GDK_NOTE (DND, g_print ("idataobject_queryinterface %#x\n", This));

  *ppvObject = NULL;

  PRINT_GUID (riid);
  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      g_print ("...IUnknown\n");
      idataobject_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IDataObject))
    {
      g_print ("...IDataObject\n");
      idataobject_addref (This);
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
idataobject_release (LPDATAOBJECT This)
{
  data_object *dobj = (data_object *) This;
  int ref_count = --dobj->ref_count;

  GDK_NOTE (DND, g_print ("idataobject_release %#x %d\n", This, ref_count));

  if (ref_count == 0)
    g_free (This);

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
idataobject_getdata (LPDATAOBJECT This,
		     LPFORMATETC  pFormatEtc,
		     LPSTGMEDIUM  pMedium)
{
  GDK_NOTE (DND, g_print ("idataobject_getdata %#x\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idataobject_getdatahere (LPDATAOBJECT This,
			 LPFORMATETC  pFormatEtc,
			 LPSTGMEDIUM  pMedium)
{
  GDK_NOTE (DND, g_print ("idataobject_getdatahere %#x\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idataobject_querygetdata (LPDATAOBJECT This,
			  LPFORMATETC  pFormatEtc)
{
  int i;

  GDK_NOTE (DND, g_print ("idataobject_querygetdata %#x %#x", This, pFormatEtc->cfFormat));

  for (i = 0; i < nformats; i++)
    if (pFormatEtc->cfFormat == formats[i].cfFormat)
      {
	GDK_NOTE (DND, g_print (" S_OK\n"));
	return S_OK;
      }

  GDK_NOTE (DND, g_print (" DV_E_FORMATETC\n"));
  return DV_E_FORMATETC;
}

static HRESULT STDMETHODCALLTYPE
idataobject_getcanonicalformatetc (LPDATAOBJECT This,
				   LPFORMATETC  pFormatEtcIn,
				   LPFORMATETC  pFormatEtcOut)
{
  GDK_NOTE (DND, g_print ("idataobject_getcanonicalformatetc %#x\n", This));

  return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_setdata (LPDATAOBJECT This,
		     LPFORMATETC  pFormatEtc,
		     LPSTGMEDIUM  pMedium,
		     BOOL         fRelease)
{
  GDK_NOTE (DND, g_print ("idataobject_setdata %#x\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idataobject_enumformatetc (LPDATAOBJECT     This,
			   DWORD            dwDirection,
			   LPENUMFORMATETC *ppEnumFormatEtc)
{
  GDK_NOTE (DND, g_print ("idataobject_enumformatetc %#x\n", This));

  if (dwDirection != DATADIR_GET)
    return E_NOTIMPL;

  *ppEnumFormatEtc = &enum_formats_new ()->ief;
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
idataobject_dadvise (LPDATAOBJECT This,
		     LPFORMATETC  pFormatetc,
		     DWORD        advf,
		     LPADVISESINK pAdvSink,
		     DWORD       *pdwConnection)
{
  GDK_NOTE (DND, g_print ("idataobject_dadvise %#x\n", This));

  return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_dunadvise (LPDATAOBJECT This,
		       DWORD         dwConnection)
{
  GDK_NOTE (DND, g_print ("idataobject_dunadvise %#x\n", This));

  return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_enumdadvise (LPDATAOBJECT    This,
			 LPENUMSTATDATA *ppenumAdvise)
{
  GDK_NOTE (DND, g_print ("idataobject_enumdadvise %#x\n", This));

  return E_FAIL;
}
		
static ULONG STDMETHODCALLTYPE
ienumformatetc_addref (LPENUMFORMATETC This)
{
  enum_formats *en = (enum_formats *) This;
  int ref_count = ++en->ref_count;

  GDK_NOTE (DND, g_print ("ienumformatetc_addref %#x %d\n", This, ref_count));

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_queryinterface (LPENUMFORMATETC This,
			       REFIID          riid,
			       LPVOID         *ppvObject)
{
  GDK_NOTE (DND, g_print ("ienumformatetc_queryinterface %#x\n", This));

  *ppvObject = NULL;

  PRINT_GUID (riid);
  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      g_print ("...IUnknown\n");
      ienumformatetc_addref (This);
      *ppvObject = This;
      return S_OK;
    }
  else if (IsEqualGUID (riid, &IID_IEnumFORMATETC))
    {
      g_print ("...IEnumFORMATETC\n");
      ienumformatetc_addref (This);
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
ienumformatetc_release (LPENUMFORMATETC This)
{
  enum_formats *en = (enum_formats *) This;
  int ref_count = --en->ref_count;

  GDK_NOTE (DND, g_print ("ienumformatetc_release %#x %d\n", This, ref_count));

  if (ref_count == 0)
    g_free (This);

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_next (LPENUMFORMATETC This,
		     ULONG	     celt,
		     LPFORMATETC     elts,
		     ULONG	    *nelt)
{
  enum_formats *en = (enum_formats *) This;
  int i, n;

  GDK_NOTE (DND, g_print ("ienumformatetc_next %#x %d %d\n", This, en->ix, celt));

  n = 0;
  for (i = 0; i < celt; i++)
    {
      if (en->ix >= nformats)
	break;
      elts[i] = formats[en->ix++];
      n++;
    }

  if (nelt != NULL)
    *nelt = n;

  if (n == celt)
    return S_OK;
  else
    return S_FALSE;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_skip (LPENUMFORMATETC This,
		     ULONG	     celt)
{
  enum_formats *en = (enum_formats *) This;

  GDK_NOTE (DND, g_print ("ienumformatetc_skip %#x %d %d\n", This, en->ix, celt));
  en->ix += celt;

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_reset (LPENUMFORMATETC This)
{
  enum_formats *en = (enum_formats *) This;

  GDK_NOTE (DND, g_print ("ienumformatetc_reset %#x\n", This));

  en->ix = 0;

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_clone (LPENUMFORMATETC  This,
		      LPENUMFORMATETC *ppEnumFormatEtc)
{
  enum_formats *en = (enum_formats *) This;
  enum_formats *new;

  GDK_NOTE (DND, g_print ("ienumformatetc_clone %#x\n", This));

  new = enum_formats_new ();

  new->ix = en->ix;

  *ppEnumFormatEtc = &new->ief;

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

static IDropSourceVtbl ids_vtbl = {
  idropsource_queryinterface,
  idropsource_addref,
  idropsource_release,
  idropsource_querycontinuedrag,
  idropsource_givefeedback
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

#endif /* OLE2_DND */

static target_drag_context *
target_context_new (void)
{
  target_drag_context *result;

  result = g_new0 (target_drag_context, 1);

#ifdef OLE2_DND
  result->idt.lpVtbl = &idt_vtbl;
#endif

  result->context = gdk_drag_context_new ();
  result->context->is_source = FALSE;

  GDK_NOTE (DND, g_print ("target_context_new: %#x\n", result));

  return result;
}

static source_drag_context *
source_context_new (void)
{
  source_drag_context *result;

  result = g_new0 (source_drag_context, 1);

#ifdef OLE2_DND
  result->ids.lpVtbl = &ids_vtbl;
#endif

  result->context = gdk_drag_context_new ();
  result->context->is_source = TRUE;

  GDK_NOTE (DND, g_print ("source_context_new: %#x\n", result));

  return result;
}

#ifdef OLE2_DND
static data_object *
data_object_new (void)
{
  data_object *result;

  result = g_new0 (data_object, 1);

  result->ido.lpVtbl = &ido_vtbl;
  result->ref_count = 1;

  GDK_NOTE (DND, g_print ("data_object_new: %#x\n", result));

  return result;
}


static enum_formats *
enum_formats_new (void)
{
  enum_formats *result;

  result = g_new0 (enum_formats, 1);

  result->ief.lpVtbl = &ief_vtbl;
  result->ref_count = 1;
  result->ix = 0;

  GDK_NOTE (DND, g_print ("enum_formats_new: %#x\n", result));

  return result;
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
resolve_link(HWND    hWnd,
	     LPCTSTR lpszLinkName,
	     LPSTR   lpszPath,
	     LPSTR   lpszDescription)
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
      context->source_window = gdk_parent_root;
      context->dest_window = event->any.window;
      gdk_drawable_ref (context->dest_window);
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
      gdk_sel_prop_store (gdk_parent_root, text_uri_list_atom, 8,
			  result->str, result->len + 1);

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

  nformats = 2;
  formats = g_new (FORMATETC, nformats);

  formats[0].cfFormat = CF_TEXT;
  formats[0].ptd = NULL;
  formats[0].dwAspect = DVASPECT_CONTENT;
  formats[0].lindex = -1;
  formats[0].tymed = TYMED_HGLOBAL;
  
  formats[1].cfFormat = CF_GDIOBJFIRST;
  formats[1].ptd = NULL;
  formats[1].dwAspect = DVASPECT_CONTENT;
  formats[1].lindex = -1;
  formats[1].tymed = TYMED_HGLOBAL;
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
gdk_drag_do_leave (GdkDragContext *context,
		   guint32         time)
{
  if (context->dest_window)
    {
      GDK_NOTE (DND, g_print ("gdk_drag_do_leave\n"));
      gdk_drawable_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

GdkDragContext * 
gdk_drag_begin (GdkWindow *window,
		GList     *targets)
{
  GList *tmp_list;
  source_drag_context *ctx;
#ifdef OLE2_DND
  data_object *dobj;
  HRESULT hResult;
  DWORD dwEffect;
  HGLOBAL global;
  FORMATETC format;
  STGMEDIUM medium;
#endif

  g_return_val_if_fail (window != NULL, NULL);

  GDK_NOTE (DND, g_print ("gdk_drag_begin\n"));

  ctx = source_context_new ();
  ctx->context->protocol = GDK_DRAG_PROTO_OLE2;
  ctx->context->source_window = window;
  gdk_drawable_ref (window);

#ifdef OLE2_DND
  tmp_list = g_list_last (targets);
  ctx->context->targets = NULL;
  while (tmp_list)
    {
      ctx->context->targets = g_list_prepend (ctx->context->targets,
					      tmp_list->data);
      tmp_list = tmp_list->prev;
    }

  ctx->context->actions = 0;

  dobj = data_object_new ();

  global = GlobalAlloc (GMEM_FIXED, sizeof (ctx));

  memcpy (&global, ctx, sizeof (ctx));

  medium.tymed = TYMED_HGLOBAL;
  medium.hGlobal = global;
  medium.pUnkForRelease = NULL;

  dobj->ido.lpVtbl->SetData (&dobj->ido, &formats[1], &medium, TRUE);

  hResult = DoDragDrop (&dobj->ido, &ctx->ids, DROPEFFECT_MOVE|DROPEFFECT_COPY, &dwEffect);

  GDK_NOTE (DND, g_print ("DoDragDrop returned %s\n",
			  (hResult == DRAGDROP_S_DROP ? "DRAGDROP_S_DROP" :
			   (hResult == DRAGDROP_S_CANCEL ? "DRAGDROP_S_CANCEL" :
			    (hResult == E_UNEXPECTED ? "E_UNEXPECTED" :
			     g_strdup_printf ("%#.8x", hResult))))));

  dobj->ido.lpVtbl->Release (&dobj->ido);
  ctx->ids.lpVtbl->Release (&ctx->ids);
#endif
  return ctx->context;
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
  GdkDrawablePrivate *drag_window_private = (GdkDrawablePrivate*) drag_window;
  HWND recipient;
  POINT pt;

  GDK_NOTE (DND, g_print ("gdk_drag_find_window: %#x +%d+%d\n",
			  (drag_window ? GDK_DRAWABLE_XID (drag_window) : 0),
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
	gdk_drawable_ref (*dest_window);
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
  GDK_NOTE (DND, g_print ("gdk_drag_motion\n"));

  return FALSE;
}

void
gdk_drag_drop (GdkDragContext *context,
	       guint32         time)
{
  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drag_drop\n"));
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
  GDK_NOTE (DND, g_print ("gdk_drag_status\n"));
}

void 
gdk_drop_reply (GdkDragContext *context,
		gboolean        ok,
		guint32         time)
{
  GDK_NOTE (DND, g_print ("gdk_drop_reply\n"));
}

void
gdk_drop_finish (GdkDragContext *context,
		 gboolean        success,
		 guint32         time)
{
  GDK_NOTE (DND, g_print ("gdk_drop_finish\n"));
}

static GdkFilterReturn
gdk_destroy_filter (GdkXEvent *xev,
		    GdkEvent  *event,
		    gpointer   data)
{
#ifdef OLE2_DND
  MSG *msg = (MSG *) xev;

  if (msg->message == WM_DESTROY)
    {
      IDropTarget *idtp = (IDropTarget *) data;

      GDK_NOTE (DND, g_print ("gdk_destroy_filter: WM_DESTROY: %#x\n", msg->hwnd));
#if 0
      idtp->lpVtbl->Release (idtp);
#endif
      RevokeDragDrop (msg->hwnd);
      CoLockObjectExternal (idtp, FALSE, TRUE);
    }
#endif
  return GDK_FILTER_CONTINUE;
}

void
gdk_window_register_dnd (GdkWindow *window)
{
#ifdef OLE2_DND
  target_drag_context *ctx;
  HRESULT hres;
#endif

  g_return_if_fail (window != NULL);

  GDK_NOTE (DND, g_print ("gdk_window_register_dnd: %#x\n",
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
  ctx = target_context_new ();
  ctx->context->protocol = GDK_DRAG_PROTO_OLE2;
  hres = CoLockObjectExternal ((IUnknown *) &ctx->idt, TRUE, FALSE);
  if (!SUCCEEDED (hres))
    OTHER_API_FAILED ("CoLockObjectExternal");
  else
    {
      hres = RegisterDragDrop (GDK_DRAWABLE_XID (window), &ctx->idt);
      if (hres == DRAGDROP_E_ALREADYREGISTERED)
	{
	  g_print ("DRAGDROP_E_ALREADYREGISTERED\n");
#if 0
	  ctx->idt.lpVtbl->Release (&ctx->idt);
#endif
	  CoLockObjectExternal ((IUnknown *) &ctx->idt, FALSE, FALSE);
	}
      else if (!SUCCEEDED (hres))
	OTHER_API_FAILED ("RegisterDragDrop");
      else
	{
	  gdk_window_add_filter (window, gdk_destroy_filter, &ctx->idt);
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
  if (context->protocol == GDK_DRAG_PROTO_WIN32_DROPFILES)
    return gdk_win32_dropfiles_atom;
  else if (context->protocol == GDK_DRAG_PROTO_OLE2)
    return gdk_ole2_dnd_atom;
  else
    return GDK_NONE;
}
