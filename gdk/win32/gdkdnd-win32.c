/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
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

typedef struct _GdkDragContextPrivateWin32 GdkDragContextPrivateWin32;

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
  g_print ("guid = %.08lx-%.04x-%.04x-%.02x%.02x-%.02x%.02x%.02x%.02x%.02x%.02x", \
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
struct _GdkDragContextPrivateWin32 {
#ifdef OLE2_DND
  gint    ref_count;
#endif
  guint16 last_x;		/* Coordinates from last event */
  guint16 last_y;
  HWND dest_xid;
  guint drag_status : 4;	/* Current status of drag */
  guint drop_failed : 1;	/* Whether the drop was unsuccessful */
};

#define GDK_DRAG_CONTEXT_PRIVATE_DATA(context) ((GdkDragContextPrivateWin32 *) GDK_DRAG_CONTEXT (context)->windowing_data)

static GdkDragContext *current_dest_drag = NULL;

static void gdk_drag_context_init       (GdkDragContext      *dragcontext);
static void gdk_drag_context_class_init (GdkDragContextClass *klass);
static void gdk_drag_context_finalize   (GObject              *object);

static gpointer parent_class = NULL;
static GList *contexts;

GType
gdk_drag_context_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkDragContextClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_drag_context_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDragContext),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_drag_context_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkDragContext",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_drag_context_init (GdkDragContext *dragcontext)
{
  GdkDragContextPrivateWin32 *private = g_new0 (GdkDragContextPrivateWin32, 1);

  dragcontext->windowing_data = private;
#ifdef OLE2_DND
  private->ref_count = 1;
#endif

  contexts = g_list_prepend (contexts, dragcontext);
}

static void
gdk_drag_context_class_init (GdkDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_drag_context_finalize;
}

static void
gdk_drag_context_finalize (GObject *object)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);
  GdkDragContextPrivateWin32 *private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);
 
  GDK_NOTE (DND, g_print ("gdk_drag_context_finalize\n"));
 
  g_list_free (context->targets);

  if (context->source_window)
    {
      g_object_unref (context->source_window);
    }
  
  if (context->dest_window)
    g_object_unref (context->dest_window);
  
  contexts = g_list_remove (contexts, context);

  if (context == current_dest_drag)
    current_dest_drag = NULL;

  g_free (private);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* Drag Contexts */

GdkDragContext *
gdk_drag_context_new (void)
{
  return g_object_new (gdk_drag_context_get_type (), NULL);
}

void
gdk_drag_context_ref (GdkDragContext *context)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  g_object_ref (context);
}

void
gdk_drag_context_unref (GdkDragContext *context)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  g_object_unref (context);
}

static GdkDragContext *
gdk_drag_context_find (gboolean   is_source,
		       GdkWindow *source,
		       GdkWindow *dest)
{
  GList *tmp_list = contexts;
  GdkDragContext *context;
  GdkDragContextPrivateWin32 *private;

  while (tmp_list)
    {
      context = (GdkDragContext *)tmp_list->data;
      private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);

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
  GdkDragContextPrivateWin32 *private = GDK_DRAG_CONTEXT_PRIVATE_DATA (ctx->context);
  int ref_count = ++private->ref_count;

  gdk_drag_context_ref (ctx->context);
  GDK_NOTE (DND, g_print ("idroptarget_addref %p %d\n", This, ref_count));
  
  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_queryinterface (LPDROPTARGET This,
			    REFIID       riid,
			    LPVOID      *ppvObject)
{
  GDK_NOTE (DND, g_print ("idroptarget_queryinterface %p\n", This));

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
  GdkDragContextPrivateWin32 *private = GDK_DRAG_CONTEXT_PRIVATE_DATA (ctx->context);
  int ref_count = --private->ref_count;

  gdk_drag_context_unref (ctx->context);
  GDK_NOTE (DND, g_print ("idroptarget_release %p %d\n", This, ref_count));

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
  GDK_NOTE (DND, g_print ("idroptarget_dragenter %p\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_dragover (LPDROPTARGET This,
		      DWORD        grfKeyState,
		      POINTL       pt,
		      LPDWORD      pdwEffect)
{
  GDK_NOTE (DND, g_print ("idroptarget_dragover %p\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_dragleave (LPDROPTARGET This)
{
  GDK_NOTE (DND, g_print ("idroptarget_dragleave %p\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idroptarget_drop (LPDROPTARGET This,
		  LPDATAOBJECT pDataObj,
		  DWORD        grfKeyState,
		  POINTL       pt,
		  LPDWORD      pdwEffect)
{
  GDK_NOTE (DND, g_print ("idroptarget_drop %p\n", This));

  return E_UNEXPECTED;
}

static ULONG STDMETHODCALLTYPE
idropsource_addref (LPDROPSOURCE This)
{
  source_drag_context *ctx = (source_drag_context *) This;
  GdkDragContextPrivateWin32 *private = GDK_DRAG_CONTEXT_PRIVATE_DATA (ctx->context);

  gdk_drag_context_ref (ctx->context);
  GDK_NOTE (DND, g_print ("idropsource_addref %p %d\n",
			  This, private->ref_count));
  
  return private->ref_count;
}

static HRESULT STDMETHODCALLTYPE
idropsource_queryinterface (LPDROPSOURCE This,
			    REFIID       riid,
			    LPVOID      *ppvObject)
{
  GDK_NOTE (DND, g_print ("idropsource_queryinterface %p\n", This));

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
  GdkDragContextPrivateWin32 *private = GDK_DRAG_CONTEXT_PRIVATE_DATA (ctx->context);
  int ref_count = --private->ref_count;

  gdk_drag_context_unref (ctx->context);
  GDK_NOTE (DND, g_print ("idropsource_release %p %d\n", This, ref_count));

  if (ref_count == 0)
    g_free (This);

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
idropsource_querycontinuedrag (LPDROPSOURCE This,
			       BOOL         fEscapePressed,
			       DWORD        grfKeyState)
{
  GDK_NOTE (DND, g_print ("idropsource_querycontinuedrag %p\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idropsource_givefeedback (LPDROPSOURCE This,
			  DWORD        dwEffect)
{
  GDK_NOTE (DND, g_print ("idropsource_givefeedback %p\n", This));

  return E_UNEXPECTED;
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
  GDK_NOTE (DND, g_print ("idataobject_queryinterface %p\n", This));

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

  GDK_NOTE (DND, g_print ("idataobject_release %p %d\n", This, ref_count));

  if (ref_count == 0)
    g_free (This);

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
idataobject_getdata (LPDATAOBJECT This,
		     LPFORMATETC  pFormatEtc,
		     LPSTGMEDIUM  pMedium)
{
  GDK_NOTE (DND, g_print ("idataobject_getdata %p\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idataobject_getdatahere (LPDATAOBJECT This,
			 LPFORMATETC  pFormatEtc,
			 LPSTGMEDIUM  pMedium)
{
  GDK_NOTE (DND, g_print ("idataobject_getdatahere %p\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idataobject_querygetdata (LPDATAOBJECT This,
			  LPFORMATETC  pFormatEtc)
{
  int i;

  GDK_NOTE (DND, g_print ("idataobject_querygetdata %p %#x", This, pFormatEtc->cfFormat));

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
  GDK_NOTE (DND, g_print ("idataobject_getcanonicalformatetc %p\n", This));

  return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_setdata (LPDATAOBJECT This,
		     LPFORMATETC  pFormatEtc,
		     LPSTGMEDIUM  pMedium,
		     BOOL         fRelease)
{
  GDK_NOTE (DND, g_print ("idataobject_setdata %p\n", This));

  return E_UNEXPECTED;
}

static HRESULT STDMETHODCALLTYPE
idataobject_enumformatetc (LPDATAOBJECT     This,
			   DWORD            dwDirection,
			   LPENUMFORMATETC *ppEnumFormatEtc)
{
  GDK_NOTE (DND, g_print ("idataobject_enumformatetc %p\n", This));

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
  GDK_NOTE (DND, g_print ("idataobject_dadvise %p\n", This));

  return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_dunadvise (LPDATAOBJECT This,
		       DWORD         dwConnection)
{
  GDK_NOTE (DND, g_print ("idataobject_dunadvise %p\n", This));

  return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE
idataobject_enumdadvise (LPDATAOBJECT    This,
			 LPENUMSTATDATA *ppenumAdvise)
{
  GDK_NOTE (DND, g_print ("idataobject_enumdadvise %p\n", This));

  return E_FAIL;
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
  GDK_NOTE (DND, g_print ("ienumformatetc_queryinterface %p\n", This));

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

  GDK_NOTE (DND, g_print ("ienumformatetc_release %p %d\n", This, ref_count));

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

  GDK_NOTE (DND, g_print ("ienumformatetc_next %p %d %ld\n", This, en->ix, celt));

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

  GDK_NOTE (DND, g_print ("ienumformatetc_skip %p %d %ld\n", This, en->ix, celt));
  en->ix += celt;

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_reset (LPENUMFORMATETC This)
{
  enum_formats *en = (enum_formats *) This;

  GDK_NOTE (DND, g_print ("ienumformatetc_reset %p\n", This));

  en->ix = 0;

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ienumformatetc_clone (LPENUMFORMATETC  This,
		      LPENUMFORMATETC *ppEnumFormatEtc)
{
  enum_formats *en = (enum_formats *) This;
  enum_formats *new;

  GDK_NOTE (DND, g_print ("ienumformatetc_clone %p\n", This));

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


static target_drag_context *
target_context_new (void)
{
  target_drag_context *result;

  result = g_new0 (target_drag_context, 1);

  result->idt.lpVtbl = &idt_vtbl;

  result->context = gdk_drag_context_new ();
  result->context->is_source = FALSE;

  GDK_NOTE (DND, g_print ("target_context_new: %p\n", result));

  return result;
}

static source_drag_context *
source_context_new (void)
{
  source_drag_context *result;

  result = g_new0 (source_drag_context, 1);

  result->ids.lpVtbl = &ids_vtbl;

  result->context = gdk_drag_context_new ();
  result->context->is_source = TRUE;

  GDK_NOTE (DND, g_print ("source_context_new: %p\n", result));

  return result;
}

static data_object *
data_object_new (void)
{
  data_object *result;

  result = g_new0 (data_object, 1);

  result->ido.lpVtbl = &ido_vtbl;
  result->ref_count = 1;

  GDK_NOTE (DND, g_print ("data_object_new: %p\n", result));

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

  GDK_NOTE (DND, g_print ("enum_formats_new: %p\n", result));

  return result;
}

#endif

/* From MS Knowledge Base article Q130698 */

static HRESULT 
resolve_link (HWND     hWnd,
	      guchar  *lpszLinkName,
	      guchar **lpszPath)
{
  HRESULT hres;
  IShellLinkA *pslA = NULL;
  IShellLinkW *pslW = NULL;
  IPersistFile *ppf = NULL;

  /* Assume failure to start with: */
  *lpszPath = 0;

  /* Call CoCreateInstance to obtain the IShellLink interface
   * pointer. This call fails if CoInitialize is not called, so it is
   * assumed that CoInitialize has been called.
   */

  if (G_WIN32_HAVE_WIDECHAR_API ())
    hres = CoCreateInstance (&CLSID_ShellLink,
			     NULL,
			     CLSCTX_INPROC_SERVER,
			     &IID_IShellLinkW,
			     (LPVOID *)&pslW);
  else
    hres = CoCreateInstance (&CLSID_ShellLink,
			     NULL,
			     CLSCTX_INPROC_SERVER,
			     &IID_IShellLinkA,
			     (LPVOID *)&pslA);

  if (SUCCEEDED (hres))
   {
     
     /* The IShellLink interface supports the IPersistFile
      * interface. Get an interface pointer to it.
      */
     if (G_WIN32_HAVE_WIDECHAR_API ())
       hres = pslW->lpVtbl->QueryInterface (pslW,
					    &IID_IPersistFile,
					    (LPVOID *) &ppf);
     else
       hres = pslA->lpVtbl->QueryInterface (pslA,
					    &IID_IPersistFile,
					    (LPVOID *) &ppf);
   }     

  if (SUCCEEDED (hres))
    {
      /* Convert the given link name string to wide character string. */
      wchar_t *wsz = g_utf8_to_utf16 (lpszLinkName, -1, NULL, NULL, NULL);

      /* Load the file. */
      hres = ppf->lpVtbl->Load (ppf, wsz, STGM_READ);
      g_free (wsz);
    }
  
  if (SUCCEEDED (hres))
    {
      /* Resolve the link by calling the Resolve()
       * interface function.
       */
      if (G_WIN32_HAVE_WIDECHAR_API ())
	hres = pslW->lpVtbl->Resolve (pslW, hWnd, SLR_ANY_MATCH | SLR_NO_UI);
      else
	hres = pslA->lpVtbl->Resolve (pslA, hWnd, SLR_ANY_MATCH | SLR_NO_UI);
    }

  if (SUCCEEDED (hres))
    {
      if (G_WIN32_HAVE_WIDECHAR_API ())
	{
	  wchar_t wtarget[MAX_PATH];

	  hres = pslW->lpVtbl->GetPath (pslW, wtarget, MAX_PATH, NULL, 0);
	  if (SUCCEEDED (hres))
	    *lpszPath = g_utf16_to_utf8 (wtarget, -1, NULL, NULL, NULL);
	}
      else
	{
	  guchar cptarget[MAX_PATH];

	  hres = pslA->lpVtbl->GetPath (pslA, cptarget, MAX_PATH, NULL, 0);
	  if (SUCCEEDED (hres))
	    *lpszPath = g_locale_to_utf8 (cptarget, -1, NULL, NULL, NULL);
	}
    }
  
  if (ppf)
    ppf->lpVtbl->Release (ppf);
  if (pslW)
    pslW->lpVtbl->Release (pslW);
  if (pslA)
    pslA->lpVtbl->Release (pslA);

  return SUCCEEDED (hres);
}

static GdkFilterReturn
gdk_dropfiles_filter (GdkXEvent *xev,
		      GdkEvent  *event,
		      gpointer   data)
{
  GdkDragContext *context;
  GdkDragContextPrivateWin32 *private;
  GString *result;
  MSG *msg = (MSG *) xev;
  HANDLE hdrop;
  POINT pt;
  gint nfiles, i;
  guchar *fileName, *linkedFile;
  
  if (msg->message == WM_DROPFILES)
    {
      GDK_NOTE (DND, g_print ("WM_DROPFILES: %p\n", msg->hwnd));

      context = gdk_drag_context_new ();
      private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);
      context->protocol = GDK_DRAG_PROTO_WIN32_DROPFILES;
      context->is_source = FALSE;
      context->source_window = _gdk_root;
      g_object_ref (context->source_window);
      context->dest_window = event->any.window;
      g_object_ref (context->dest_window);
      /* WM_DROPFILES drops are always file names */
      context->targets =
	g_list_append (NULL, GUINT_TO_POINTER (_text_uri_list));
      context->actions = GDK_ACTION_COPY;
      context->suggested_action = GDK_ACTION_COPY;
      current_dest_drag = context;

      event->dnd.type = GDK_DROP_START;
      event->dnd.context = current_dest_drag;
      
      hdrop = (HANDLE) msg->wParam;
      DragQueryPoint (hdrop, &pt);
      ClientToScreen (msg->hwnd, &pt);

      event->dnd.x_root = pt.x + _gdk_offset_x;
      event->dnd.y_root = pt.y + _gdk_offset_y;
      event->dnd.time = _gdk_win32_get_next_tick (msg->time);

      nfiles = DragQueryFile (hdrop, 0xFFFFFFFF, NULL, 0);

      result = g_string_new (NULL);
      for (i = 0; i < nfiles; i++)
	{
	  gchar *uri;

	  if (G_WIN32_HAVE_WIDECHAR_API ())
	    {
	      wchar_t wfn[MAX_PATH];

	      DragQueryFileW (hdrop, i, wfn, MAX_PATH);
	      fileName = g_utf16_to_utf8 (wfn, -1, NULL, NULL, NULL);
	    }
	  else
	    {
	      char cpfn[MAX_PATH];

	      DragQueryFileA (hdrop, i, cpfn, MAX_PATH);
	      fileName = g_locale_to_utf8 (cpfn, -1, NULL, NULL, NULL);
	    }

	  /* Resolve shortcuts */
	  if (resolve_link (msg->hwnd, fileName, &linkedFile))
	    {
	      uri = g_filename_to_uri (linkedFile, NULL, NULL);
	      g_free (linkedFile);
	      if (uri != NULL)
		{
		  g_string_append (result, uri);
		  GDK_NOTE (DND, g_print ("... %s link to %s: %s\n",
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
		  GDK_NOTE (DND, g_print ("... %s: %s\n", fileName, uri));
		  g_free (uri);
		}
	    }
	  g_free (fileName);
	  g_string_append (result, "\015\012");
	}
      _gdk_dropfiles_store (result->str);
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
_gdk_dnd_init (void)
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
_gdk_win32_dnd_exit (void)
{
#ifdef OLE2_DND
  OleUninitialize ();
#endif
}

/* Source side */

static void
local_send_leave (GdkDragContext *context,
		  guint32         time)
{
  GdkEvent tmp_event;
  
  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      tmp_event.dnd.type = GDK_DRAG_LEAVE;
      tmp_event.dnd.window = context->dest_window;
      /* Pass ownership of context to the event */
      tmp_event.dnd.send_event = FALSE;
      tmp_event.dnd.context = current_dest_drag;
      tmp_event.dnd.time = GDK_CURRENT_TIME; /* FIXME? */

      current_dest_drag = NULL;
      
      gdk_event_put (&tmp_event);
    }
}

static void
local_send_enter (GdkDragContext *context,
		  guint32         time)
{
  GdkEvent tmp_event;
  GdkDragContextPrivateWin32 *private;
  GdkDragContext *new_context;

  private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);
  
  if (current_dest_drag != NULL)
    {
      gdk_drag_context_unref (current_dest_drag);
      current_dest_drag = NULL;
    }

  new_context = gdk_drag_context_new ();
  new_context->protocol = GDK_DRAG_PROTO_LOCAL;
  new_context->is_source = FALSE;

  new_context->source_window = context->source_window;
  g_object_ref (new_context->source_window);
  new_context->dest_window = context->dest_window;
  g_object_ref (new_context->dest_window);

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
  
  gdk_event_put (&tmp_event);
}

static void
local_send_motion (GdkDragContext *context,
		   gint            x_root, 
		   gint            y_root,
		   GdkDragAction   action,
		   guint32         time)
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

      (GDK_DRAG_CONTEXT_PRIVATE_DATA (current_dest_drag))->last_x = x_root;
      (GDK_DRAG_CONTEXT_PRIVATE_DATA (current_dest_drag))->last_y = y_root;

      GDK_DRAG_CONTEXT_PRIVATE_DATA (context)->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;
      
      gdk_event_put (&tmp_event);
    }
}

static void
local_send_drop (GdkDragContext *context,
		 guint32         time)
{
  GdkEvent tmp_event;
  
  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      GdkDragContextPrivateWin32 *private;
      private = GDK_DRAG_CONTEXT_PRIVATE_DATA (current_dest_drag);

      /* Pass ownership of context to the event */
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

      g_object_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

GdkDragContext * 
gdk_drag_begin (GdkWindow *window,
		GList     *targets)
{
#ifndef OLE2_DND
  GList *tmp_list;
  GdkDragContext *new_context;

  g_return_val_if_fail (window != NULL, NULL);

  new_context = gdk_drag_context_new ();
  new_context->is_source = TRUE;
  new_context->source_window = window;
  g_object_ref (window);

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
#else
  source_drag_context *ctx;
  GList *tmp_list;
  data_object *dobj;
  HRESULT hResult;
  DWORD dwEffect;
  HGLOBAL global;
  STGMEDIUM medium;

  g_return_val_if_fail (window != NULL, NULL);

  GDK_NOTE (DND, g_print ("gdk_drag_begin\n"));

  ctx = source_context_new ();
  ctx->context->protocol = GDK_DRAG_PROTO_OLE2;
  ctx->context->source_window = window;
  g_object_ref (window);

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
			     g_strdup_printf ("%#.8lx", hResult))))));

  dobj->ido.lpVtbl->Release (&dobj->ido);
  ctx->ids.lpVtbl->Release (&ctx->ids);

  return ctx->context;
#endif
}

guint32
gdk_drag_get_protocol_for_display (GdkDisplay      *display,
				   guint32          xid,
				   GdkDragProtocol *protocol)
{
  GdkWindow *window;

  GDK_NOTE (DND, g_print ("gdk_drag_get_protocol\n"));

  window = gdk_window_lookup (xid);

  if (GPOINTER_TO_INT (gdk_drawable_get_data (window, "gdk-dnd-registered")))
    {
      *protocol = GDK_DRAG_PROTO_LOCAL;
      return xid;
    }

  return 0;
}

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

void
gdk_drag_find_window_for_screen (GdkDragContext  *context,
				 GdkWindow       *drag_window,
				 GdkScreen       *screen,
				 gint             x_root,
				 gint             y_root,
				 GdkWindow      **dest_window,
				 GdkDragProtocol *protocol)
{
  find_window_enum_arg a;

  a.x = x_root - _gdk_offset_x;
  a.y = y_root - _gdk_offset_y;
  a.ignore = drag_window ? GDK_WINDOW_HWND (drag_window) : NULL;
  a.result = NULL;

  EnumWindows (find_window_enum_proc, (LPARAM) &a);

  if (a.result == NULL)
    *dest_window = NULL;
  else
    {
      *dest_window = gdk_win32_handle_table_lookup (GPOINTER_TO_UINT (a.result));
      if (*dest_window)
	{
	  *dest_window = gdk_window_get_toplevel (*dest_window);
	  g_object_ref (*dest_window);
	}

      if (context->source_window)
        *protocol = GDK_DRAG_PROTO_LOCAL;
      else
        *protocol = GDK_DRAG_PROTO_WIN32_DROPFILES;
    }

  GDK_NOTE (DND,
	    g_print ("gdk_drag_find_window: %p %+d%+d: %p: %p %d\n",
		     (drag_window ? GDK_WINDOW_HWND (drag_window) : NULL),
		     x_root, y_root,
		     a.result,
		     (*dest_window ? GDK_WINDOW_HWND (*dest_window) : NULL),
		     *protocol));
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
  GdkDragContextPrivateWin32 *private;

  g_return_val_if_fail (context != NULL, FALSE);

  GDK_NOTE (DND, g_print ("gdk_drag_motion\n"));

  private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);
  
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
	  g_object_ref (context->dest_window);
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
  GdkDragContextPrivateWin32 *private;
  GdkDragContext *src_context;
  GdkEvent tmp_event;

  g_return_if_fail (context != NULL);

  private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);

  context->action = action;

  src_context = gdk_drag_context_find (TRUE,
				       context->source_window,
				       context->dest_window);

  if (src_context)
    {
      GdkDragContextPrivateWin32 *private = GDK_DRAG_CONTEXT_PRIVATE_DATA (src_context);
      
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
	  _gdk_dropfiles_store (NULL);
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
  GdkDragContextPrivateWin32 *private;
  GdkDragContext *src_context;
  GdkEvent tmp_event;
	
  g_return_if_fail (context != NULL);

  GDK_NOTE (DND, g_print ("gdk_drop_finish"));

  private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);

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
#if 0
      idtp->lpVtbl->Release (idtp);
#endif
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
  target_drag_context *ctx;
  HRESULT hres;
#endif

  g_return_if_fail (window != NULL);

  if (GPOINTER_TO_INT (gdk_drawable_get_data (window, "gdk-dnd-registered")))
    return;

  gdk_drawable_set_data (window, "gdk-dnd-registered", GINT_TO_POINTER(TRUE), NULL);

  GDK_NOTE (DND, g_print ("gdk_window_register_dnd: %p\n",
			  GDK_WINDOW_HWND (window)));

  /* We always claim to accept dropped files, but in fact we might not,
   * of course. This function is called in such a way that it cannot know
   * whether the window (widget) in question actually accepts files
   * (in gtk, data of type text/uri-list) or not.
   */
  gdk_window_add_filter (window, gdk_dropfiles_filter, NULL);
  DragAcceptFiles (GDK_WINDOW_HWND (window), TRUE);

#ifdef OLE2_DND
  /* Register for OLE2 d&d */
  ctx = target_context_new ();
  ctx->context->protocol = GDK_DRAG_PROTO_OLE2;
  hres = CoLockObjectExternal ((IUnknown *) &ctx->idt, TRUE, FALSE);
  if (!SUCCEEDED (hres))
    OTHER_API_FAILED ("CoLockObjectExternal");
  else
    {
      hres = RegisterDragDrop (GDK_WINDOW_HWND (window), &ctx->idt);
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
  if (context->protocol == GDK_DRAG_PROTO_LOCAL)
    return _local_dnd;
  else if (context->protocol == GDK_DRAG_PROTO_WIN32_DROPFILES)
    return _gdk_win32_dropfiles;
  else if (context->protocol == GDK_DRAG_PROTO_OLE2)
    return _gdk_ole2_dnd;
  else
    return GDK_NONE;
}

gboolean 
gdk_drag_drop_succeeded (GdkDragContext *context)
{
  GdkDragContextPrivateWin32 *private;

  g_return_val_if_fail (context != NULL, FALSE);

  private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);

  /* FIXME: Can we set drop_failed when the drop has failed? */
  return !private->drop_failed;
}
