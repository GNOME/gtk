/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include <X11/Xlocale.h>
#include "gdk.h"
#include "gdkprivate.h"
#include "gdki18n.h"
#include "gdkx.h"

#if HAVE_CONFIG_H
#  include <config.h>
#  if STDC_HEADERS
#    include <string.h>
#  endif
#endif

#ifdef USE_NATIVE_LOCALE
#include <stdlib.h>
#endif

/* If this variable is FALSE, it indicates that we should
 * avoid trying to use multibyte conversion functions and
 * assume everything is 1-byte per character
 */
static gboolean gdk_use_mb;

#ifdef USE_XIM

#include <stdarg.h>
#include <X11/Xresource.h>

/* The following routines duplicate functionality in Xlib to
 * translate from varargs to X's internal opaque XVaNestedList.
 * 
 * If all vendors have stuck close to the reference implementation,
 * then we should hopefully be OK. 
 */

typedef struct {
  gchar	  *name;
  gpointer value;
} GdkImArg;

#ifdef USE_X11R6_XIM
static void   gdk_im_instantiate_cb      (Display *display,
 					  XPointer client_data,
 					  XPointer call_data);
#endif
static void   gdk_im_destroy_cb          (XIM im,
     					  XPointer client_data,
 					  XPointer call_data);
static gint   gdk_im_real_open           (void);
static void   gdk_ic_real_new            (GdkIC *ic);

static GdkICAttributesType gdk_ic_real_set_attr (GdkIC *ic,
						 GdkICAttr           *attr,
						 GdkICAttributesType  mask);

static XIM        xim_im;			/* global IM */
static XIMStyles* xim_styles;			/* im supports these styles */
static XIMStyle xim_best_allowed_style;
static GList* xim_ic_list;

#endif /* USE_XIM */

/*
 *--------------------------------------------------------------
 * gdk_set_locale
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gchar*
gdk_set_locale (void)
{
  wchar_t result;
  gchar *current_locale;

  gdk_use_mb = FALSE;

  if (!setlocale (LC_ALL,""))
    g_warning ("locale not supported by C library");
  
  if (!XSupportsLocale ())
    {
      g_warning ("locale not supported by Xlib, locale set to C");
      setlocale (LC_ALL, "C");
    }
  
  if (!XSetLocaleModifiers (""))
    g_warning ("can not set locale modifiers");

  current_locale = setlocale (LC_ALL, NULL);

  if ((strcmp (current_locale, "C")) && (strcmp (current_locale, "POSIX")))
    {
      gdk_use_mb = TRUE;

#ifndef X_LOCALE
      /* Detect GNU libc, where mb == UTF8. Not useful unless it's
       * really a UTF8 locale. The below still probably will
       * screw up on Greek, Cyrillic, etc, encoded as UTF8.
       */
      
      if ((MB_CUR_MAX == 2) &&
	  (mbstowcs (&result, "\xdd\xa5", 1) > 0) &&
	  result == 0x765)
	{
	  if ((strlen (current_locale) < 4) ||
	      g_strcasecmp (current_locale + strlen(current_locale) - 4, "utf8"))
	    gdk_use_mb = FALSE;
	}
#endif /* X_LOCALE */
    }

  GDK_NOTE (XIM,
	    g_message ("%s multi-byte string functions.", 
		       gdk_use_mb ? "Using" : "Not using"));
  
  return current_locale;
}

#ifdef USE_XIM

/*
 *--------------------------------------------------------------
 * gdk_im_begin
 *
 *   Begin using input method with XIM Protocol(X11R6 standard)
 *
 * Arguments:
 *   "ic" is the "Input Context" which is created by gtk_ic_new.
 *   The input area is specified with "window".
 *
 * Results:
 *   The gdk's event handling routine is switched to XIM based routine.
 *   XIM based routine uses XFilterEvent to get rid of events used by IM,
 *   and uses XmbLookupString instead of XLookupString.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void 
gdk_im_begin (GdkIC *ic, GdkWindow* window)
{
  GdkICPrivate *private;
  GdkICAttr attr;
  
  g_return_if_fail (ic != NULL);
  
  private = (GdkICPrivate *) ic;
  
  attr.focus_window = window;
  gdk_ic_set_attr (ic, &attr, GDK_IC_FOCUS_WINDOW);

  if (private != gdk_xim_ic)
    {
      gdk_im_end();
      if (private->xic)
	{
	  XSetICFocus (private->xic);
	  GDK_NOTE (XIM, g_message ("im_begin icfocus : %p(%ld)\n",
				    private->xic,
				    GDK_WINDOW_XWINDOW(private->attr->focus_window)));
	}
    }
  gdk_xim_ic = private;
  gdk_xim_window = window;
}

/*
 *--------------------------------------------------------------
 * gdk_im_end
 *
 *   End using input method with XIM Protocol(X11R6 standard)
 *
 * Arguments:
 *
 * Results:
 *   The gdk's event handling routine is switched to normal routine.
 *   User should call this function before ic and window will be destroyed.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void 
gdk_im_end (void)
{
  if (gdk_xim_ic && gdk_xim_ic->xic)
    {
      XUnsetICFocus (gdk_xim_ic->xic);
      GDK_NOTE (XIM, g_message ("im_end unfocus : %p\n", gdk_xim_ic->xic));
    }
  gdk_xim_ic = NULL;
  gdk_xim_window = NULL;
}

static GdkIMStyle 
gdk_im_choose_better_style (GdkIMStyle style1, GdkIMStyle style2) 
{
  GdkIMStyle s1, s2, u;
  
  if (style1 == 0) return style2;
  if (style2 == 0) return style1;
  if ((style1 & (GDK_IM_PREEDIT_MASK | GDK_IM_STATUS_MASK))
    	== (style2 & (GDK_IM_PREEDIT_MASK | GDK_IM_STATUS_MASK)))
    return style1;

  s1 = style1 & GDK_IM_PREEDIT_MASK;
  s2 = style2 & GDK_IM_PREEDIT_MASK;
  u = s1 | s2;
  if (s1 != s2) {
    if (u & GDK_IM_PREEDIT_CALLBACKS)
      return (s1 == GDK_IM_PREEDIT_CALLBACKS)? style1:style2;
    else if (u & GDK_IM_PREEDIT_POSITION)
      return (s1 == GDK_IM_PREEDIT_POSITION)? style1:style2;
    else if (u & GDK_IM_PREEDIT_AREA)
      return (s1 == GDK_IM_PREEDIT_AREA)? style1:style2;
    else if (u & GDK_IM_PREEDIT_NOTHING)
      return (s1 == GDK_IM_PREEDIT_NOTHING)? style1:style2;
  } else {
    s1 = style1 & GDK_IM_STATUS_MASK;
    s2 = style2 & GDK_IM_STATUS_MASK;
    u = s1 | s2;
    if ( u & GDK_IM_STATUS_CALLBACKS)
      return (s1 == GDK_IM_STATUS_CALLBACKS)? style1:style2;
    else if ( u & GDK_IM_STATUS_AREA)
      return (s1 == GDK_IM_STATUS_AREA)? style1:style2;
    else if ( u & GDK_IM_STATUS_NOTHING)
      return (s1 == GDK_IM_STATUS_NOTHING)? style1:style2;
    else if ( u & GDK_IM_STATUS_NONE)
      return (s1 == GDK_IM_STATUS_NONE)? style1:style2;
  }
  return 0; /* Get rid of stupid warning */
}

GdkIMStyle
gdk_im_decide_style (GdkIMStyle supported_style)
{
  gint i;
  GdkIMStyle style, tmp;
  
  g_return_val_if_fail (xim_styles != NULL, 0);
  
  style = 0;
  for (i=0; i<xim_styles->count_styles; i++)
    {
      tmp = xim_styles->supported_styles[i];
      if (tmp == (tmp & supported_style & xim_best_allowed_style))
	style = gdk_im_choose_better_style (style, tmp);
    }
  return style;
}

GdkIMStyle
gdk_im_set_best_style (GdkIMStyle style)
{
  if (style & GDK_IM_PREEDIT_MASK)
    {
      xim_best_allowed_style &= ~GDK_IM_PREEDIT_MASK;

      xim_best_allowed_style |= GDK_IM_PREEDIT_NONE;
      if (!(style & GDK_IM_PREEDIT_NONE))
	{
	  xim_best_allowed_style |= GDK_IM_PREEDIT_NOTHING;
	  if (!(style & GDK_IM_PREEDIT_NOTHING))
	    {
	      xim_best_allowed_style |= GDK_IM_PREEDIT_AREA;
	      if (!(style & GDK_IM_PREEDIT_AREA))
		{
		  xim_best_allowed_style |= GDK_IM_PREEDIT_POSITION;
		  if (!(style & GDK_IM_PREEDIT_POSITION))
		    xim_best_allowed_style |= GDK_IM_PREEDIT_CALLBACKS;
		}
	    }
	}
    }
  if (style & GDK_IM_STATUS_MASK)
    {
      xim_best_allowed_style &= ~GDK_IM_STATUS_MASK;

      xim_best_allowed_style |= GDK_IM_STATUS_NONE;
      if (!(style & GDK_IM_STATUS_NONE))
	{
	  xim_best_allowed_style |= GDK_IM_STATUS_NOTHING;
	  if (!(style & GDK_IM_STATUS_NOTHING))
	    {
	      xim_best_allowed_style |= GDK_IM_STATUS_AREA;
	      if (!(style & GDK_IM_STATUS_AREA))
		xim_best_allowed_style |= GDK_IM_STATUS_CALLBACKS;
	    }
	}
    }
  
  return xim_best_allowed_style;
}

#ifdef USE_X11R6_XIM
static void
gdk_im_destroy_cb (XIM im, XPointer client_data, XPointer call_data)
{
  GList *node;
  GdkICPrivate *private;

  GDK_NOTE (XIM, g_message ("Ouch, Input Method is destroyed!!\n"));

  xim_im = NULL;

  if (xim_styles)
    {
      XFree (xim_styles);
      xim_styles = NULL;
    }

  for (node = xim_ic_list; node != NULL; node = g_list_next(node))
    {
      private = (GdkICPrivate *) (node->data);
      private->xic = NULL;
    }

  XRegisterIMInstantiateCallback (gdk_display, NULL, NULL, NULL,
      				  gdk_im_instantiate_cb, NULL);
}

static void
gdk_im_instantiate_cb (Display *display,
    		       XPointer client_data, XPointer call_data)
{
  GDK_NOTE (XIM, g_message ("New IM is instantiated."));
  if (display != gdk_display)
    return;

  gdk_im_real_open ();

  if (xim_im != NULL)
    XUnregisterIMInstantiateCallback (gdk_display, NULL, NULL, NULL,
				      gdk_im_instantiate_cb, NULL);
}
#endif

static gint
gdk_im_real_open (void)
{
  GList *node;

  xim_im = XOpenIM (GDK_DISPLAY(), NULL, NULL, NULL);
  if (xim_im == NULL)
    {
      GDK_NOTE (XIM, g_warning ("Unable to open IM."));
      return FALSE;
    }
  else
    {
#ifdef USE_X11R6_XIM
      XIMCallback destroy_cb;

      destroy_cb.callback = gdk_im_destroy_cb;
      destroy_cb.client_data = NULL;
      if (NULL != (void *) XSetIMValues (xim_im, XNDestroyCallback, &destroy_cb, NULL))
	GDK_NOTE (XIM, g_warning ("Could not set destroy callback to IM. Be careful to not destroy your input method."));
#endif

      XGetIMValues (xim_im, XNQueryInputStyle, &xim_styles, NULL, NULL);

      for (node = xim_ic_list; node != NULL; node = g_list_next(node))
	{
	  GdkICPrivate *private = (GdkICPrivate *) (node->data);
	  if (private->xic == NULL)
	    gdk_ic_real_new ((GdkIC *)private);
	}
      return TRUE;
    }
}

gint 
gdk_im_open (void)
{
  gdk_xim_ic = NULL;
  gdk_xim_window = (GdkWindow*)NULL;
  xim_im = NULL;
  xim_styles = NULL;

  /* initialize XIM Protocol variables */
  if (!(xim_best_allowed_style & GDK_IM_PREEDIT_MASK))
    gdk_im_set_best_style (GDK_IM_PREEDIT_CALLBACKS);
  if (!(xim_best_allowed_style & GDK_IM_STATUS_MASK))
    gdk_im_set_best_style (GDK_IM_STATUS_CALLBACKS);

  if (gdk_im_real_open ())
    return TRUE;

#ifdef USE_X11R6_XIM
  XRegisterIMInstantiateCallback (gdk_display, NULL, NULL, NULL,
				  gdk_im_instantiate_cb, NULL);
#endif

  return FALSE;
}

void 
gdk_im_close (void)
{
  if (xim_im)
    {
      XCloseIM (xim_im);
      xim_im = NULL;
    }
  if (xim_styles)
    {
      XFree (xim_styles);
      xim_styles = NULL;
    }
}

gboolean
gdk_im_ready (void)
{
  return (xim_im != NULL);
}

static void
gdk_ic_real_new (GdkIC *ic)
{
  XPoint spot_location;
  XRectangle preedit_area;
  XRectangle status_area;
  XVaNestedList *preedit_attr = NULL;
  XVaNestedList *status_attr = NULL;
  GdkICAttr *attr;
  GdkICPrivate *private;
  GdkICAttributesType mask = GDK_IC_ALL_REQ;

  private = (GdkICPrivate *) ic;
  attr = private->attr;

  switch (attr->style & GDK_IM_PREEDIT_MASK)
    {
    case GDK_IM_PREEDIT_AREA:
      mask |= GDK_IC_PREEDIT_AREA_REQ;

      preedit_area.x = attr->preedit_area.x;
      preedit_area.y = attr->preedit_area.y;
      preedit_area.width = attr->preedit_area.width;
      preedit_area.height = attr->preedit_area.height;

      preedit_attr = XVaCreateNestedList (0,
	  				  XNArea, &preedit_area,
					  XNFontSet,
					  GDK_FONT_XFONT(attr->preedit_fontset),
					  NULL);
      break;

    case GDK_IM_PREEDIT_POSITION:
      mask |= GDK_IC_PREEDIT_POSITION_REQ;

      preedit_area.x = attr->preedit_area.x;
      preedit_area.y = attr->preedit_area.y;
      preedit_area.width = attr->preedit_area.width;
      preedit_area.height = attr->preedit_area.height;

      spot_location.x = attr->spot_location.x;
      spot_location.y = attr->spot_location.y;

      preedit_attr = XVaCreateNestedList (0,
	  				  XNArea, &preedit_area,
					  XNFontSet,
					  GDK_FONT_XFONT(attr->preedit_fontset),
					  XNSpotLocation, &spot_location,
					  NULL);
      break;
    }

  switch (attr->style & GDK_IM_STATUS_MASK)
    {
    case GDK_IM_STATUS_AREA:
      mask |= GDK_IC_STATUS_AREA_REQ;

      status_area.x = attr->status_area.x;
      status_area.y = attr->status_area.y;
      status_area.width = attr->status_area.width;
      status_area.height = attr->status_area.height;

      status_attr = XVaCreateNestedList (0,
	  				 XNArea, &status_area,
					 XNFontSet,
					 GDK_FONT_XFONT(attr->status_fontset),
					 NULL);
      break;
    }

  /* We have to ensure that the client window is actually created on
   * the X server, or XCreateIC fails because the XIM server can't get
   * information about the client window.
   */
  gdk_flush();
  
  if (preedit_attr != NULL && status_attr != NULL)
    private->xic = XCreateIC (xim_im,
			      XNInputStyle,
			      attr->style,
			      XNClientWindow,
			      GDK_WINDOW_XWINDOW(attr->client_window),
			      XNPreeditAttributes,
			      preedit_attr,
			      XNStatusAttributes,
			      status_attr,
			      NULL);
  else if (preedit_attr != NULL)
    private->xic = XCreateIC (xim_im,
			      XNInputStyle,
			      attr->style,
			      XNClientWindow,
			      GDK_WINDOW_XWINDOW(attr->client_window),
			      XNPreeditAttributes,
			      preedit_attr,
			      NULL);
  else if (status_attr != NULL)
    private->xic = XCreateIC (xim_im,
			      XNInputStyle,
			      attr->style,
			      XNClientWindow,
			      GDK_WINDOW_XWINDOW(attr->client_window),
			      XNStatusAttributes,
			      status_attr,
			      NULL);
  else
    private->xic = XCreateIC (xim_im,
			      XNInputStyle,
			      attr->style,
			      XNClientWindow,
			      GDK_WINDOW_XWINDOW(attr->client_window),
			      NULL);

  if (preedit_attr)
    XFree (preedit_attr);
  if (status_attr)
    XFree (status_attr);

  if (private->xic == NULL)
    g_warning ("can not create input context with specified input style.");
  else
    gdk_ic_real_set_attr (ic, private->attr, private->mask & ~mask);
}

GdkIC *
gdk_ic_new (GdkICAttr *attr, GdkICAttributesType mask)
{
  GdkICPrivate *private;
  gboolean error = 0;
  GdkICAttributesType invalid_mask;
  GdkICAttr *pattr;

  g_return_val_if_fail (attr != NULL, NULL);
  g_return_val_if_fail ((mask & GDK_IC_ALL_REQ) == GDK_IC_ALL_REQ, NULL);

  switch (attr->style & GDK_IM_PREEDIT_MASK)
    {
    case 0:
      g_warning ("preedit style is not specified.\n");
      error = 1;
      break;

    case GDK_IM_PREEDIT_AREA:
      if ((mask & GDK_IC_PREEDIT_AREA_REQ) != GDK_IC_PREEDIT_AREA_REQ)
	error = 4;
      break;

    case GDK_IM_PREEDIT_POSITION:
      if ((mask & GDK_IC_PREEDIT_POSITION_REQ) != GDK_IC_PREEDIT_POSITION_REQ)
	error = 4;
      break;
    }

  switch (attr->style & GDK_IM_STATUS_MASK)
    {
    case 0:
      g_warning ("status style is not specified.\n");
      error = 2;
      break;

    case GDK_IM_STATUS_AREA:
      if ((mask & GDK_IC_STATUS_AREA_REQ) != GDK_IC_STATUS_AREA_REQ)
	error = 8;
      break;
    }

  if (error)
    {
      if (error & 12)
	g_warning ("IC attribute is not enough to required input style.\n");
      return NULL;
    }

  if (attr->client_window == NULL ||
      ((GdkWindowPrivate *)attr->client_window)->destroyed)
    {
      g_warning ("Client_window is null or already destroyed.\n");
      return NULL;
    }

  private = g_new0 (GdkICPrivate, 1);
  private->attr = pattr = gdk_ic_attr_new ();

  gdk_window_ref (attr->client_window);
  pattr->client_window = attr->client_window;
  pattr->style = attr->style;
  private->mask = GDK_IC_STYLE | GDK_IC_CLIENT_WINDOW;
  
  /* XIC is still not created, so following call only copies attributes */
  invalid_mask = gdk_ic_set_attr ((GdkIC *)private, attr, mask & ~GDK_IC_ALL_REQ);

  switch (attr->style & GDK_IM_PREEDIT_MASK)
    {
    case GDK_IM_PREEDIT_AREA:
      if (invalid_mask & GDK_IC_PREEDIT_AREA_REQ)
	error = TRUE;
      break;

    case GDK_IM_PREEDIT_POSITION:
      if (invalid_mask & GDK_IC_PREEDIT_POSITION_REQ)
	error = TRUE;
      break;
    }

  switch (attr->style & GDK_IM_STATUS_MASK)
    {
    case GDK_IM_STATUS_AREA:
      if (invalid_mask & GDK_IC_STATUS_AREA_REQ)
	error = TRUE;
      break;
    }

  if (error == TRUE)
    {
      g_warning ("Essential attributes for required style are invalid.\n");
      gdk_ic_destroy ((GdkIC *)private);
      return NULL;
    }

  if (gdk_im_ready ())
    gdk_ic_real_new ((GdkIC *)private);

  xim_ic_list = g_list_append (xim_ic_list, private);
  
  return (GdkIC *)private;
}

void 
gdk_ic_destroy (GdkIC *ic)
{
  GdkICPrivate *private;
  
  g_return_if_fail (ic != NULL);
  
  private = (GdkICPrivate *) ic;
  
  if (gdk_xim_ic == private)
    gdk_im_end ();
  
  GDK_NOTE (XIM, g_message ("ic_destroy %p\n", private->xic));
  if (private->xic != NULL)
    XDestroyIC (private->xic);

  if (private->attr->client_window)
    gdk_window_unref (private->attr->client_window);
  if (private->attr->focus_window)
    gdk_window_unref (private->attr->focus_window);

  if (private->attr->preedit_fontset)
    gdk_font_unref (private->attr->preedit_fontset);
  if (private->attr->preedit_pixmap)
    gdk_pixmap_unref (private->attr->preedit_pixmap);
  if (private->attr->preedit_colormap)
    gdk_colormap_unref (private->attr->preedit_colormap);

  if (private->attr->status_fontset)
    gdk_font_unref (private->attr->status_fontset);
  if (private->attr->status_pixmap)
    gdk_pixmap_unref (private->attr->status_pixmap);
  if (private->attr->status_colormap)
    gdk_colormap_unref (private->attr->status_colormap);

  xim_ic_list = g_list_remove (xim_ic_list, private);
  gdk_ic_attr_destroy (private->attr);
  g_free (private);
}

GdkIMStyle
gdk_ic_get_style (GdkIC *ic)
{
  GdkICPrivate *private;
  
  g_return_val_if_fail (ic != NULL, 0);
  
  private = (GdkICPrivate *) ic;
  
  return private->attr->style;
}

/*
 * for keeping binary compatibility if member of ic attributes is added.
 */
GdkICAttr *
gdk_ic_attr_new (void)
{
  return g_new0 (GdkICAttr, 1);
}

void
gdk_ic_attr_destroy (GdkICAttr *attr)
{
  g_return_if_fail (attr != NULL);

  g_free (attr);
}

static GdkICAttributesType
gdk_ic_real_set_attr (GdkIC *ic,
    		      GdkICAttr *attr,
		      GdkICAttributesType mask)
{
  GdkICPrivate *private = (GdkICPrivate *)ic;
  XIC xic = private->xic;
  GdkICAttributesType error = 0;
  GdkImArg arg[2] = {{NULL, NULL}, {NULL, NULL}};

  if (mask & GDK_IC_FOCUS_WINDOW)
    {
      if (XSetICValues (xic, XNFocusWindow,
			GDK_WINDOW_XWINDOW(attr->focus_window), NULL) != NULL)
	error |= GDK_IC_FOCUS_WINDOW;
    }

  if (mask & GDK_IC_SPOT_LOCATION)
    {
      XPoint point;

      point.x = attr->spot_location.x;
      point.y = attr->spot_location.y;

      arg->name = XNSpotLocation;
      arg->value = (gpointer) &point;

      if (XSetICValues (xic, XNPreeditAttributes, arg, NULL))
	error |= GDK_IC_SPOT_LOCATION;
    }

  if (mask & GDK_IC_LINE_SPACING)
    {
      arg->name = XNLineSpace;
      arg->value = GINT_TO_POINTER( attr->line_spacing );

      if (XSetICValues (xic, XNPreeditAttributes, arg, NULL))
	error |= GDK_IC_LINE_SPACING;
    }

  if (mask & GDK_IC_CURSOR)
    {
      GdkCursorPrivate *cursor = (GdkCursorPrivate *) attr->cursor;

      if (XSetICValues (xic, XNCursor, cursor->xcursor, NULL))
	error |= GDK_IC_CURSOR;
    }

  if (mask & GDK_IC_PREEDIT_FONTSET)
    {
      arg->name = XNFontSet;
      arg->value = (gpointer) GDK_FONT_XFONT(attr->preedit_fontset);

      if (XSetICValues (xic, XNPreeditAttributes, arg, NULL))
	error |= GDK_IC_PREEDIT_FONTSET;
    }

  if (mask & GDK_IC_PREEDIT_AREA)
    {
      XRectangle rect;

      rect.x = attr->preedit_area.x;
      rect.y = attr->preedit_area.y;
      rect.width = attr->preedit_area.width;
      rect.height = attr->preedit_area.height;

      arg->name = XNArea;
      arg->value = (gpointer) &rect;

      if (XSetICValues (xic, XNPreeditAttributes, arg, NULL))
	error |= GDK_IC_PREEDIT_AREA;
    }

  if (mask & GDK_IC_PREEDIT_AREA_NEEDED)
    {
      XRectangle rect;

      rect.x = attr->preedit_area_needed.x;
      rect.y = attr->preedit_area_needed.y;
      rect.width = attr->preedit_area_needed.width;
      rect.height = attr->preedit_area_needed.height;

      arg->name = XNArea;
      arg->value = (gpointer) &rect;

      if (XSetICValues (xic, XNPreeditAttributes, arg, NULL))
	error |= GDK_IC_PREEDIT_AREA_NEEDED;
      else
	private->mask &= ~GDK_IC_PREEDIT_AREA_NEEDED;
    }

  if (mask & GDK_IC_PREEDIT_FOREGROUND)
    {
      arg->name = XNForeground;
      arg->value = (gpointer) attr->preedit_foreground.pixel;

      if (XSetICValues (xic, XNPreeditAttributes, arg, NULL))
	error |= GDK_IC_PREEDIT_FOREGROUND;
    }

  if (mask & GDK_IC_PREEDIT_BACKGROUND)
    {
      arg->name = XNBackground;
      arg->value = (gpointer) attr->preedit_background.pixel;

      if (XSetICValues (xic, XNPreeditAttributes, arg, NULL))
	error |= GDK_IC_PREEDIT_BACKGROUND;
    }

  if (mask & GDK_IC_PREEDIT_PIXMAP)
    {
      arg->name = XNBackgroundPixmap;
      arg->value = (gpointer) GDK_WINDOW_XWINDOW(attr->preedit_pixmap);

      if (XSetICValues (xic, XNPreeditAttributes, arg, NULL))
	error |= GDK_IC_PREEDIT_PIXMAP;
    }

  if (mask & GDK_IC_PREEDIT_COLORMAP)
    {
      arg->name = XNColormap;
      arg->value = (gpointer) GDK_COLORMAP_XCOLORMAP(attr->preedit_colormap);

      if (XSetICValues (xic, XNPreeditAttributes, arg, NULL))
	error |= GDK_IC_PREEDIT_COLORMAP;
    }


  if (mask & GDK_IC_STATUS_FONTSET)
    {
      arg->name = XNFontSet;
      arg->value = (gpointer) GDK_FONT_XFONT(attr->status_fontset);

      if (XSetICValues (xic, XNStatusAttributes, arg, NULL))
	error |= GDK_IC_STATUS_FONTSET;
    }

  if (mask & GDK_IC_STATUS_AREA)
    {
      XRectangle rect;

      rect.x = attr->status_area.x;
      rect.y = attr->status_area.y;
      rect.width = attr->status_area.width;
      rect.height = attr->status_area.height;

      arg->name = XNArea;
      arg->value = (gpointer) &rect;

      if (XSetICValues (xic, XNStatusAttributes, arg, NULL))
	error |= GDK_IC_STATUS_AREA;
    }

  if (mask & GDK_IC_STATUS_AREA_NEEDED)
    {
      XRectangle rect;

      rect.x = attr->status_area_needed.x;
      rect.y = attr->status_area_needed.y;
      rect.width = attr->status_area_needed.width;
      rect.height = attr->status_area_needed.height;

      arg->name = XNArea;
      arg->value = (gpointer) &rect;

      if (XSetICValues (xic, XNStatusAttributes, arg, NULL))
	error |= GDK_IC_STATUS_AREA_NEEDED;
      else
	private->mask &= ~GDK_IC_STATUS_AREA_NEEDED;
    }

  if (mask & GDK_IC_STATUS_FOREGROUND)
    {
      arg->name = XNForeground;
      arg->value = (gpointer) attr->status_foreground.pixel;

      if (XSetICValues (xic, XNStatusAttributes, arg, NULL))
	error |= GDK_IC_STATUS_FOREGROUND;
    }

  if (mask & GDK_IC_STATUS_BACKGROUND)
    {
      arg->name = XNBackground;
      arg->value = (gpointer) attr->status_background.pixel;

      if (XSetICValues (xic, XNStatusAttributes, arg, NULL))
	error |= GDK_IC_STATUS_BACKGROUND;
    }

  if (mask & GDK_IC_STATUS_PIXMAP)
    {
      arg->name = XNBackgroundPixmap;
      arg->value = (gpointer) GDK_WINDOW_XWINDOW(attr->status_pixmap);

      if (XSetICValues (xic, XNStatusAttributes, arg, NULL))
	error |= GDK_IC_STATUS_PIXMAP;
    }

  if (mask & GDK_IC_STATUS_COLORMAP)
    {
      arg->name = XNColormap;
      arg->value = (gpointer) GDK_COLORMAP_XCOLORMAP(attr->status_colormap);

      if (XSetICValues (xic, XNStatusAttributes, arg, NULL))
	error |= GDK_IC_STATUS_COLORMAP;
    }

  return error;
}

GdkICAttributesType
gdk_ic_set_attr (GdkIC *ic,
    		 GdkICAttr *attr,
		 GdkICAttributesType mask)
{
  GdkICPrivate *private;
  GdkICAttr *pattr;
  GdkICAttributesType error = 0;
  GdkICAttributesType newattr = 0;

  g_return_val_if_fail (ic != NULL, 0);
  g_return_val_if_fail (attr != NULL, 0);

  private = (GdkICPrivate *) ic;
  pattr = private->attr;

  /* Check and copy new attributes */

  if (mask & GDK_IC_STYLE)
    {
      g_warning ("input style can be specified only when creating new ic.\n");
      error |= GDK_IC_STYLE;
    }

  if (mask & GDK_IC_FILTER_EVENTS)
    {
      g_warning ("filter events is read only attributes.\n");
      error |= GDK_IC_FILTER_EVENTS;
    }

  if (mask & GDK_IC_CLIENT_WINDOW)
    {
      g_warning ("client window can be specified only when creating new ic.\n");
      error |= GDK_IC_CLIENT_WINDOW;
    }

  if (mask & GDK_IC_FOCUS_WINDOW)
    {
      if (attr->focus_window == NULL)
	{
	  g_warning ("specified focus_window is invalid.\n");
	  error |= GDK_IC_FOCUS_WINDOW;
	}
      else if (pattr->focus_window != attr->focus_window)
	{
	  if (pattr->focus_window != NULL)
	    gdk_window_unref (pattr->focus_window);
	  if (attr->focus_window != NULL)
	    gdk_window_ref (attr->focus_window);
	  pattr->focus_window = attr->focus_window;
	  newattr |= GDK_IC_FOCUS_WINDOW;
	}
    }

  if (mask & GDK_IC_SPOT_LOCATION)
    {
      pattr->spot_location = attr->spot_location;
      newattr |= GDK_IC_SPOT_LOCATION;
    }

  if (mask & GDK_IC_LINE_SPACING)
    {
      pattr->line_spacing = attr->line_spacing;
      newattr |= GDK_IC_LINE_SPACING;
    }

  if (mask & GDK_IC_CURSOR)
    {
      pattr->cursor = attr->cursor;
      newattr |= GDK_IC_CURSOR;
    }

  if (mask & GDK_IC_PREEDIT_FONTSET)
    {
      if (attr->preedit_fontset == NULL ||
	  attr->preedit_fontset->type != GDK_FONT_FONTSET)
	{
	  g_warning ("gdk_font is NULL or not a fontset.\n");
	  error |= GDK_IC_PREEDIT_FONTSET;
	}
      else if (pattr->preedit_fontset != attr->preedit_fontset)
	{
	  if (pattr->preedit_fontset != NULL)
	    gdk_font_unref (pattr->preedit_fontset);
	  if (attr->preedit_fontset != NULL)
	    gdk_font_ref (attr->preedit_fontset);
	  pattr->preedit_fontset = attr->preedit_fontset;
	  newattr |= GDK_IC_PREEDIT_FONTSET;
	}
    }

  if (mask & GDK_IC_PREEDIT_AREA)
    {
      pattr->preedit_area = attr->preedit_area;
      newattr |= GDK_IC_PREEDIT_AREA;
    }

  if (mask & GDK_IC_PREEDIT_AREA_NEEDED)
    {
      if (attr->preedit_area_needed.width == 0 ||
	  attr->preedit_area_needed.height == 0)
	{
	  g_warning ("width and height of preedit_area_needed must be non 0.\n");
	  error |= GDK_IC_PREEDIT_AREA_NEEDED;
	}
      else
	{
	  pattr->preedit_area_needed = attr->preedit_area_needed;
	  newattr |= GDK_IC_PREEDIT_AREA_NEEDED;
	}
    }

  if (mask & GDK_IC_PREEDIT_FOREGROUND)
    {
      pattr->preedit_foreground = attr->preedit_foreground;
      newattr |= GDK_IC_PREEDIT_FOREGROUND;
    }

  if (mask & GDK_IC_PREEDIT_BACKGROUND)
    {
      pattr->preedit_background = attr->preedit_background;
      newattr |= GDK_IC_PREEDIT_BACKGROUND;
    }

  if (mask & GDK_IC_PREEDIT_PIXMAP)
    {
      if (attr->preedit_pixmap != NULL &&
	  ((GdkPixmapPrivate *)attr->preedit_pixmap)->destroyed)
	{
	  g_warning ("Preedit pixmap is already destroyed.\n");
	  error |= GDK_IC_PREEDIT_PIXMAP;
	}
      else
	{
	  if (pattr->preedit_pixmap != attr->preedit_pixmap)
	    {
	      if (pattr->preedit_pixmap != NULL)
		gdk_pixmap_unref (pattr->preedit_pixmap);
	      if (attr->preedit_pixmap)
		gdk_pixmap_ref (attr->preedit_pixmap);
	      pattr->preedit_pixmap = attr->preedit_pixmap;
	      newattr |= GDK_IC_PREEDIT_PIXMAP;
	    }
	}
    }

  if (mask & GDK_IC_PREEDIT_COLORMAP)
    {
      if (pattr->preedit_colormap != attr->preedit_colormap)
	{
	  if (pattr->preedit_colormap != NULL)
	    gdk_colormap_unref (pattr->preedit_colormap);
	  if (attr->preedit_colormap != NULL)
	    gdk_colormap_ref (attr->preedit_colormap);
	  pattr->preedit_colormap = attr->preedit_colormap;
	  newattr |= GDK_IC_PREEDIT_COLORMAP;
	}
    }

  if (mask & GDK_IC_STATUS_FONTSET)
    {
      if (attr->status_fontset == NULL ||
	  attr->status_fontset->type != GDK_FONT_FONTSET)
	{
	  g_warning ("gdk_font is NULL or not a fontset.\n");
	  error |= GDK_IC_STATUS_FONTSET;
	}
      else if (pattr->status_fontset != attr->status_fontset)
	{
	  if (pattr->status_fontset != NULL)
	    gdk_font_unref (pattr->status_fontset);
	  if (attr->status_fontset != NULL)
	    gdk_font_ref (attr->status_fontset);
	  pattr->status_fontset = attr->status_fontset;
	  newattr |= GDK_IC_STATUS_FONTSET;
	}
    }

  if (mask & GDK_IC_STATUS_AREA)
    {
      pattr->status_area = attr->status_area;
      newattr |= GDK_IC_STATUS_AREA;
    }

  if (mask & GDK_IC_STATUS_AREA_NEEDED)
    {
      if (attr->status_area_needed.width == 0 ||
	  attr->status_area_needed.height == 0)
	{
	  g_warning ("width and height of status_area_needed must be non 0.\n");
	  error |= GDK_IC_STATUS_AREA_NEEDED;
	}
      else
	{
	  pattr->status_area_needed = attr->status_area_needed;
	  newattr |= GDK_IC_STATUS_AREA_NEEDED;
	}
    }

  if (mask & GDK_IC_STATUS_FOREGROUND)
    {
      pattr->status_foreground = attr->status_foreground;
      newattr |= GDK_IC_STATUS_FOREGROUND;
    }

  if (mask & GDK_IC_STATUS_BACKGROUND)
    {
      pattr->status_background = attr->status_background;
      newattr |= GDK_IC_STATUS_BACKGROUND;
    }

  if (mask & GDK_IC_STATUS_PIXMAP)
    {
      if (attr->status_pixmap != NULL &&
	  ((GdkPixmapPrivate *)attr->status_pixmap)->destroyed)
	{
	  g_warning ("Preedit pixmap is already destroyed.\n");
	  error |= GDK_IC_STATUS_PIXMAP;
	}
      else
	{
	  if (pattr->status_pixmap != attr->status_pixmap)
	    {
	      if (pattr->status_pixmap != NULL)
		gdk_pixmap_unref (pattr->status_pixmap);
	      if (attr->status_pixmap)
		gdk_pixmap_ref (attr->status_pixmap);
	      pattr->status_pixmap = attr->status_pixmap;
	      newattr |= GDK_IC_STATUS_PIXMAP;
	    }
	}
    }

  if (mask & GDK_IC_STATUS_COLORMAP)
    {
      if (pattr->status_colormap != attr->status_colormap)
	{
	  if (pattr->status_colormap != NULL)
	    gdk_colormap_unref (pattr->status_colormap);
	  if (attr->status_colormap != NULL)
	    gdk_colormap_ref (attr->status_colormap);
	  pattr->status_colormap = attr->status_colormap;
	  newattr |= GDK_IC_STATUS_COLORMAP;
	}
    }

  if (private->xic == NULL)
    return error;

  error |= gdk_ic_real_set_attr (ic, pattr, newattr);

  return error;
}

GdkICAttributesType
gdk_ic_get_attr (GdkIC *ic,
    		 GdkICAttr *attr,
		 GdkICAttributesType mask)
{
  GdkICPrivate *private;
  GdkICAttr *pattr;
  GdkICAttributesType known, unknown = 0;

  g_return_val_if_fail (ic != NULL, -1);
  g_return_val_if_fail (attr != NULL, -1);

  private = (GdkICPrivate *) ic;
  pattr = private->attr;

  known = mask & private->mask;

  if (known & GDK_IC_STYLE)
    attr->style = pattr->style;
  if (known & GDK_IC_CLIENT_WINDOW)
    attr->client_window = pattr->client_window;
  if (known & GDK_IC_FOCUS_WINDOW)
    attr->focus_window = pattr->focus_window;
  if (known & GDK_IC_FILTER_EVENTS)
    attr->filter_events = pattr->filter_events;
  if (known & GDK_IC_LINE_SPACING)
    attr->line_spacing = pattr->line_spacing;
  if (known & GDK_IC_CURSOR)
    attr->cursor = pattr->cursor;

  if (known & GDK_IC_PREEDIT_FONTSET)
    attr->preedit_fontset = pattr->preedit_fontset;
  if (known & GDK_IC_PREEDIT_AREA)
    attr->preedit_area = pattr->preedit_area;
  if (known & GDK_IC_PREEDIT_AREA_NEEDED)
    attr->preedit_area_needed = pattr->preedit_area_needed;
  if (known & GDK_IC_PREEDIT_FOREGROUND)
    attr->preedit_foreground = pattr->preedit_foreground;
  if (known & GDK_IC_PREEDIT_BACKGROUND)
    attr->preedit_background = pattr->preedit_background;
  if (known & GDK_IC_PREEDIT_PIXMAP)
    attr->preedit_pixmap = pattr->preedit_pixmap;
  if (known & GDK_IC_PREEDIT_COLORMAP)
    attr->preedit_colormap = pattr->preedit_colormap;

  if (known & GDK_IC_STATUS_FONTSET)
    attr->status_fontset = pattr->status_fontset;
  if (known & GDK_IC_STATUS_AREA)
    attr->status_area = pattr->status_area;
  if (known & GDK_IC_STATUS_AREA_NEEDED)
    attr->status_area_needed = pattr->status_area_needed;
  if (known & GDK_IC_STATUS_FOREGROUND)
    attr->status_foreground = pattr->status_foreground;
  if (known & GDK_IC_STATUS_BACKGROUND)
    attr->status_background = pattr->status_background;
  if (known & GDK_IC_STATUS_PIXMAP)
    attr->status_pixmap = pattr->status_pixmap;
  if (known & GDK_IC_STATUS_COLORMAP)
    attr->status_colormap = pattr->status_colormap;

  if (private->xic)
    {
      unknown = mask & ~(private->mask);

      if (unknown & GDK_IC_FOCUS_WINDOW)
	attr->focus_window = pattr->client_window;
      if (unknown & GDK_IC_FILTER_EVENTS)
	{
	  gdk_ic_get_events (ic);
	  attr->filter_events = pattr->filter_events;
	}
      if (mask & GDK_IC_SPOT_LOCATION)
	{
	  XPoint point;
	  XVaNestedList *list;
	  
	  list = XVaCreateNestedList (0, XNSpotLocation, &point, NULL);
	  if (XGetICValues (private->xic, XNPreeditAttributes, list, NULL))
	    unknown &= ~GDK_IC_SPOT_LOCATION;
	  else
	    {
	      pattr->spot_location.x = point.x;
	      pattr->spot_location.y = point.y;
	      private->mask |= GDK_IC_SPOT_LOCATION;

	      attr->spot_location = pattr->spot_location;
	    }
	  XFree (list);
	}
      if (unknown & GDK_IC_PREEDIT_AREA_NEEDED)
	{
	  XRectangle rect;
	  XVaNestedList *list;

	  list = XVaCreateNestedList (0, XNAreaNeeded, &rect, NULL);
	  if (XGetICValues (private->xic, XNPreeditAttributes, list, NULL))
	    unknown &= ~GDK_IC_PREEDIT_AREA_NEEDED;
	  else
	    {
	      pattr->preedit_area_needed.x = rect.x;
	      pattr->preedit_area_needed.y = rect.y;
	      pattr->preedit_area_needed.width = rect.width;
	      pattr->preedit_area_needed.height = rect.height;
	      private->mask |= GDK_IC_PREEDIT_AREA_NEEDED;

	      attr->preedit_area = pattr->preedit_area;
	    }
	  XFree (list);
	}
      if (unknown & GDK_IC_STATUS_AREA_NEEDED)
	{
	  XRectangle rect;
	  XVaNestedList *list;

	  list = XVaCreateNestedList (0, XNAreaNeeded, &rect, NULL);
	  if (XGetICValues (private->xic, XNStatusAttributes, list, NULL))
	    unknown &= ~GDK_IC_STATUS_AREA_NEEDED;
	  else
	    {
	      pattr->status_area_needed.x = rect.x;
	      pattr->status_area_needed.y = rect.y;
	      pattr->status_area_needed.width = rect.width;
	      pattr->status_area_needed.height = rect.height;
	      private->mask |= GDK_IC_STATUS_AREA_NEEDED;

	      attr->status_area = pattr->status_area;
	    }
	  XFree (list);
	}
    }

  return mask & ~known & ~unknown;
}

GdkEventMask 
gdk_ic_get_events (GdkIC *ic)
{
  GdkEventMask mask;
  glong xmask;
  glong bit;
  GdkICPrivate *private;
  gint i;
  
  /*  From gdkwindow.c	*/
  
  g_return_val_if_fail (ic != NULL, 0);
  
  private = (GdkICPrivate *) ic;

  if (private->mask & GDK_IC_FILTER_EVENTS)
    return private->attr->filter_events;
  
  if (XGetICValues (private->xic, XNFilterEvents, &xmask, NULL) != NULL)
    {
      GDK_NOTE (XIM, g_warning ("Call to XGetICValues: %s failed", XNFilterEvents));
      return 0;
    }
  
  mask = 0;
  for (i=0, bit=2; i < gdk_nevent_masks; i++, bit <<= 1)
    if (xmask & gdk_event_mask_table [i])
      {
	mask |= bit;
	xmask &= ~ gdk_event_mask_table [i];
      }
  
  if (xmask)
    g_warning ("ic requires events not supported by the application (%#04lx)", xmask);
  
  private->attr->filter_events = mask;
  private->mask |= GDK_IC_FILTER_EVENTS;

  return mask;
}

void 
gdk_ic_cleanup (void)
{
  gint destroyed;
  
  destroyed = 0;
  while (xim_ic_list != NULL)
    {
      gdk_ic_destroy ((GdkIC *) xim_ic_list->data);
      destroyed ++;
    }
#ifdef G_ENABLE_DEBUG
  if ((gdk_debug_flags & GDK_DEBUG_XIM) && destroyed > 0)
    {
      g_warning ("Cleaned up %i IC(s)\n", destroyed);
    }
#endif /* G_ENABLE_DEBUG */
}

#else /* !USE_XIM */

void 
gdk_im_begin (GdkIC *ic, GdkWindow* window)
{
}

void 
gdk_im_end (void)
{
}

GdkIMStyle
gdk_im_decide_style (GdkIMStyle supported_style)
{
  return GDK_IM_PREEDIT_NONE | GDK_IM_STATUS_NONE;
}

GdkIMStyle
gdk_im_set_best_style (GdkIMStyle style)
{
  return GDK_IM_PREEDIT_NONE | GDK_IM_STATUS_NONE;
}

gint 
gdk_im_ready (void)
{
  return FALSE;
}

GdkIC * 
gdk_ic_new (GdkICAttr *attr, GdkICAttributesType mask)
{
  return NULL;
}

void 
gdk_ic_destroy (GdkIC *ic)
{
}

GdkIMStyle
gdk_ic_get_style (GdkIC *ic)
{
  return GDK_IM_PREEDIT_NONE | GDK_IM_STATUS_NONE;
}

void 
gdk_ic_set_values (GdkIC *ic, ...)
{
}

void 
gdk_ic_get_values (GdkIC *ic, ...)
{
}

GdkICAttributesType 
gdk_ic_set_attr (GdkIC *ic, GdkICAttr *attr, GdkICAttributesType mask)
{
  return 0;
}

GdkICAttributesType 
gdk_ic_get_attr (GdkIC *ic, GdkICAttr *attr, GdkICAttributesType mask)
{
  return 0;
}

GdkEventMask 
gdk_ic_get_events (GdkIC *ic)
{
  return 0;
}

#endif /* USE_XIM */

/*
 * gdk_wcstombs_len
 *
 * Returns a multi-byte string converted from the specified array
 * of wide characters. The string is newly allocated. The array of
 * wide characters is nul-terminated, if len < 0
 */

#ifdef USE_NATIVE_LOCALE
gchar *
_gdk_wcstombs_len (const GdkWChar *src,
		   gint            src_len)
{
  gint len = 0;
  gint i;
  gchar *result = NULL;
  gchar buf[16];
  gchar *p;

  if (MB_CUR_MAX <= 16)
    p = buf;
  else
    p = g_malloc (MB_CUR_MAX);	/* Presumably never hit */

  wctomb (NULL, 0);

  for (i=0; (src_len < 0 || i < src_len) && src[i]; i++)
    {
      gint charlen = wctomb (p, src[i]);
      if (charlen < 0)
	goto out;
      
      len += charlen;
    }

  result = g_malloc (len + 1);

  /* Old versions of GNU libc apparently can't handle a max of 0 here
   */
  if (len > 0)
    wcstombs (result, (wchar_t *)src, len);
  
  result[len] = '\0';

 out:
  if (p != buf)
    g_free (p);

  return result;
}
#else /* !USE_NATIVE_LOCALE */

gchar *
_gdk_wcstombs_len (const GdkWChar *src,
		   int             len)
{
  gchar *mbstr = NULL;
  gint length;
  
  if (len < 0)
    {
      length = 0;

      while (src[length] != 0)
	length++;
    }
  else
    length = len;

  if (gdk_use_mb)
    {
      XTextProperty tpr;
      wchar_t *src_wc;

      /* The len < 0 part is to ensure nul termination
       */
      if (len < 0 && sizeof(wchar_t) == sizeof(GdkWChar))
	{
	  src_wc = (wchar_t *)src;
	}
      else
	{
	  gint i;

	  src_wc = g_new (wchar_t, length + 1);

	  for (i = 0; i < length; i++)
	    src_wc[i] = src[i];

	  src_wc[i] = 0;
	}
      
      if (XwcTextListToTextProperty (gdk_display, &src_wc, 1,
				     XTextStyle, &tpr) == Success)
	{
	  /*
	   * We must copy the string into an area allocated by glib, because
	   * the string 'tpr.value' must be freed by XFree().
	   */
	  mbstr = g_strdup(tpr.value);
	  XFree (tpr.value);
	}

      if (src_wc != (wchar_t *)src)
	g_free (src_wc);
    }
  else
    {
      gint i;

      mbstr = g_new (gchar, length + 1);

      for (i=0; i < length; i++)
	mbstr[i] = src[i];

      mbstr[i] = '\0';
    }

  return mbstr;
}
#endif /* !USE_NATIVE_LOCALE */
  
/*
 * gdk_wcstombs 
 *
 * Returns a multi-byte string converted from the specified array
 * of wide characters. The string is newly allocated. The array of
 * wide characters must be null-terminated. If the conversion is
 * failed, it returns NULL.
 */
gchar *
gdk_wcstombs (const GdkWChar *src)
{
  return _gdk_wcstombs_len (src, -1);
}
  
/*
 * gdk_mbstowcs
 *
 * Converts the specified string into wide characters, and, returns the
 * number of wide characters written. The string 'src' must be
 * null-terminated. If the conversion is failed, it returns -1.
 */
gint
gdk_mbstowcs (GdkWChar *dest, const gchar *src, gint dest_max)
{
#ifdef USE_NATIVE_LOCALE
  return mbstowcs ((wchar_t *)dest, src, dest_max);
#else
  if (gdk_use_mb)
    {
      XTextProperty tpr;
      wchar_t **wstrs, *wstr_src;
      gint num_wstrs;
      gint len_cpy;
      if (XmbTextListToTextProperty (gdk_display, (char **)&src, 1, XTextStyle,
				     &tpr)
	  != Success)
	{
	  /* NoMem or LocaleNotSupp */
	  return -1;
	}
      if (XwcTextPropertyToTextList (gdk_display, &tpr, &wstrs, &num_wstrs)
	  != Success)
	{
	  /* InvalidChar */
	  XFree(tpr.value);
	  return -1;
	}
      XFree(tpr.value);
      if (num_wstrs == 0)
	return 0;
      wstr_src = wstrs[0];
      for (len_cpy=0; len_cpy<dest_max && wstr_src[len_cpy]; len_cpy++)
	dest[len_cpy] = wstr_src[len_cpy];
      XwcFreeStringList (wstrs);
      return len_cpy;
    }
  else
    {
      gint i;
      
      for (i=0; i<dest_max && src[i]; i++)
	dest[i] = (guchar)src[i];
      
      return i;
    }
#endif
}
