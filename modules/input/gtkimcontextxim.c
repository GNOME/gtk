/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

#include "locale.h"
#include <string.h>

#include "gtk/gtksignal.h"
#include "gtkimcontextxim.h"

struct _GtkXIMInfo
{
  XIM im;
  char *locale;
  XIMStyle style;
};

static void     gtk_im_context_xim_class_init         (GtkIMContextXIMClass  *class);
static void     gtk_im_context_xim_init               (GtkIMContextXIM       *im_context_xim);
static void     gtk_im_context_xim_finalize           (GObject               *obj);
static void     gtk_im_context_xim_set_client_window  (GtkIMContext          *context,
						       GdkWindow             *client_window);
static gboolean gtk_im_context_xim_filter_keypress    (GtkIMContext          *context,
						       GdkEventKey           *key);
static void     gtk_im_context_xim_reset              (GtkIMContext          *context);
static void     gtk_im_context_xim_get_preedit_string (GtkIMContext          *context,
						       gchar                **str,
						       PangoAttrList        **attrs,
						       gint                  *cursor_pos);

static XIC       gtk_im_context_xim_get_ic            (GtkIMContextXIM *context_xim);
static GObjectClass *parent_class;

GType gtk_type_im_context_xim = 0;

static GSList *open_ims = NULL;

void
gtk_im_context_xim_register_type (GTypeModule *type_module)
{
  static const GTypeInfo im_context_xim_info =
  {
    sizeof (GtkIMContextXIMClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) gtk_im_context_xim_class_init,
    NULL,           /* class_finalize */    
    NULL,           /* class_data */
    sizeof (GtkIMContextXIM),
    0,
    (GtkObjectInitFunc) gtk_im_context_xim_init,
  };

  gtk_type_im_context_xim = 
    g_type_module_register_type (type_module,
				 GTK_TYPE_IM_CONTEXT,
				 "GtkIMContextXIM",
				 &im_context_xim_info, 0);
}

#define PREEDIT_MASK (XIMPreeditCallbacks | XIMPreeditPosition | \
		      XIMPreeditArea | XIMPreeditNothing | XIMPreeditNone)
#define STATUS_MASK (XIMStatusCallbacks | XIMStatusArea | \
		      XIMStatusNothing | XIMStatusNone)
#define ALLOWED_MASK (XIMPreeditCallbacks | XIMPreeditNothing | XIMPreeditNone | \
		      XIMStatusCallbacks | XIMStatusNothing | XIMStatusNone)

static XIMStyle 
choose_better_style (XIMStyle style1, XIMStyle style2) 
{
  XIMStyle s1, s2, u; 
  
  if (style1 == 0) return style2;
  if (style2 == 0) return style1;
  if ((style1 & (PREEDIT_MASK | STATUS_MASK))
    	== (style2 & (PREEDIT_MASK | STATUS_MASK)))
    return style1;

  s1 = style1 & PREEDIT_MASK;
  s2 = style2 & PREEDIT_MASK;
  u = s1 | s2;
  if (s1 != s2) {
    if (u & XIMPreeditCallbacks)
      return (s1 == XIMPreeditCallbacks) ? style1 : style2;
    else if (u & XIMPreeditPosition)
      return (s1 == XIMPreeditPosition) ? style1 :style2;
    else if (u & XIMPreeditArea)
      return (s1 == XIMPreeditArea) ? style1 : style2;
    else if (u & XIMPreeditNothing)
      return (s1 == XIMPreeditNothing) ? style1 : style2;
  } else {
    s1 = style1 & STATUS_MASK;
    s2 = style2 & STATUS_MASK;
    u = s1 | s2;
    if (u & XIMStatusCallbacks)
      return (s1 == XIMStatusCallbacks) ? style1 : style2;
    else if (u & XIMStatusArea)
      return (s1 == XIMStatusArea) ? style1 : style2;
    else if (u & XIMStatusNothing)
      return (s1 == XIMStatusNothing) ? style1 : style2;
    else if (u & XIMStatusNone)
      return (s1 == XIMStatusNone) ? style1 : style2;
  }
  return 0; /* Get rid of stupid warning */
}

static void
setup_im (GtkXIMInfo *info)
{
  XIMStyles *xim_styles;
  XIMValuesList *ic_values;
  int i;
  
  XGetIMValues (info->im,
		XNQueryInputStyle, &xim_styles,
		XNQueryICValuesList, &ic_values);

  info->style = 0;
  for (i = 0; i < xim_styles->count_styles; i++)
    if ((xim_styles->supported_styles[i] & ALLOWED_MASK) == xim_styles->supported_styles[i])
      info->style = choose_better_style (info->style,
					 xim_styles->supported_styles[i]);

#if 0
  for (i = 0; i < ic_values->count_values; i++)
    g_print ("%s\n", ic_values->supported_values[i]);
  for (i = 0; i < xim_styles->count_styles; i++)
    g_print ("%#x\n", xim_styles->supported_styles[i]);
#endif

  XFree (xim_styles);
  XFree (ic_values);
}

static GtkXIMInfo *
get_im (const char *locale)
{
  GSList *tmp_list = open_ims;
  GtkXIMInfo *info;
  XIM im = NULL;

  while (tmp_list)
    {
      info = tmp_list->data;
      if (!strcmp (info->locale, locale))
	return info;

      tmp_list = tmp_list->next;
    }

  info = NULL;

  if (XSupportsLocale ())
    {
      if (!XSetLocaleModifiers (""))
	g_warning ("can not set locale modifiers");
      
      im = XOpenIM (GDK_DISPLAY(), NULL, NULL, NULL);
      
      if (im)
	{
	  info = g_new (GtkXIMInfo, 1);
	  open_ims = g_slist_prepend (open_ims, im);
	  
	  info->locale = g_strdup (locale);
	  info->im = im;

	  setup_im (info);
	}
    }

  return info;
}

static void
gtk_im_context_xim_class_init (GtkIMContextXIMClass *class)
{
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  im_context_class->set_client_window = gtk_im_context_xim_set_client_window;
  im_context_class->filter_keypress = gtk_im_context_xim_filter_keypress;
  im_context_class->reset = gtk_im_context_xim_reset;
  im_context_class->get_preedit_string = gtk_im_context_xim_get_preedit_string;
  gobject_class->finalize = gtk_im_context_xim_finalize;
}

static void
gtk_im_context_xim_init (GtkIMContextXIM *im_context_xim)
{
}

static void
gtk_im_context_xim_finalize (GObject *obj)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (obj);

  if (context_xim->ic)
    {
      XDestroyIC (context_xim->ic);
      context_xim->ic = NULL;
    }
 
  g_free (context_xim->mb_charset);
}

static void
gtk_im_context_xim_set_client_window (GtkIMContext          *context,
				      GdkWindow             *client_window)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (context);

  if (context_xim->ic)
    {
      XDestroyIC (context_xim->ic);
      context_xim->ic = NULL;
    }

  context_xim->client_window = client_window;
}

GtkIMContext *
gtk_im_context_xim_new (void)
{
  GtkXIMInfo *info;
  GtkIMContextXIM *result;
  gchar *charset;

  info = get_im (setlocale (LC_CTYPE, NULL));
  if (!info)
    return NULL;

  result = GTK_IM_CONTEXT_XIM (gtk_type_new (GTK_TYPE_IM_CONTEXT_XIM));

  result->im_info = info;
  
  g_get_charset (&charset);
  result->mb_charset = g_strdup (charset);

  return GTK_IM_CONTEXT (result);
}

static char *
mb_to_utf8 (GtkIMContextXIM *context_xim,
	    const char      *str)
{
  GError *error = NULL;
  gchar *result;

  result = g_convert (str, -1,
		      "UTF-8", context_xim->mb_charset,
		      NULL, NULL, &error);

  if (!result)
    {
      g_warning ("Error converting text from IM to UTF-8: %s\n", error->message);
      g_error_free (error);
    }
  
  return result;
}

static gboolean
gtk_im_context_xim_filter_keypress (GtkIMContext *context,
				    GdkEventKey  *event)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (context);
  XIC ic = gtk_im_context_xim_get_ic (context_xim);
  gchar static_buffer[256];
  gchar *buffer = static_buffer;
  gint buffer_size = sizeof(static_buffer) - 1;
  gint num_bytes = 0;
  KeySym keysym;
  Status status;
  gboolean result = FALSE;

  XKeyPressedEvent xevent;

  if (!ic)
    return FALSE;

  xevent.type = KeyPress;
  xevent.serial = 0;		/* hope it doesn't matter */
  xevent.send_event = event->send_event;
  xevent.display = GDK_DRAWABLE_XDISPLAY (event->window);
  xevent.window = GDK_DRAWABLE_XID (event->window);
  xevent.root = GDK_ROOT_WINDOW();
  xevent.subwindow = xevent.window;
  xevent.time = event->time;
  xevent.x = xevent.x_root = 0;
  xevent.y = xevent.y_root = 0;
  xevent.state = event->state;
  xevent.keycode = event->keyval ? XKeysymToKeycode (xevent.display, event->keyval) : 0;
  xevent.same_screen = True;
  
  if (XFilterEvent ((XEvent *)&xevent, GDK_DRAWABLE_XID (context_xim->client_window)))
    return TRUE;
  
 again:
  num_bytes = XmbLookupString (ic, &xevent, buffer, buffer_size, &keysym, &status);

  if (status == XBufferOverflow)
    {
      buffer_size = num_bytes;
      buffer = g_malloc (num_bytes + 1);
      goto again;
    }

  /* I don't know how we should properly handle XLookupKeysym or XLookupBoth
   * here ... do input methods actually change the keysym? we can't really
   * feed it back to accelerator processing at this point...
   */
  if (status == XLookupChars || status == XLookupBoth)
    {
      char *result_utf8;

      buffer[num_bytes] = '\0';

      result_utf8 = mb_to_utf8 (context_xim, buffer);
      if (result_utf8)
	{
	  if ((guchar)result_utf8[0] >= 0x20) /* Some IM have a nasty habit of converting
					       * control characters into strings
					       */
	    {
	      gtk_signal_emit_by_name (GTK_OBJECT (context), "commit", result_utf8);
	      result = TRUE;
	    }
	  
	  g_free (result_utf8);
	}
    }

  return FALSE;
}

static void
gtk_im_context_xim_reset (GtkIMContext *context)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (context);
  XIC ic = gtk_im_context_xim_get_ic (context_xim);
  gchar *result;

  if (!ic)
    return;
  
  result = XmbResetIC (ic);

  if (result)
    {
      char *result_utf8 = mb_to_utf8 (context_xim, result);
      if (result_utf8)
	{
	  gtk_signal_emit_by_name (GTK_OBJECT (context), "commit", result_utf8);
	  g_free (result_utf8);
	}
    }

  if (context_xim->preedit_length)
    {
      context_xim->preedit_length = 0;
      gtk_signal_emit_by_name (GTK_OBJECT (context), "preedit-changed");
    }

  XFree (result);
}

/* Mask of feedback bits that we render
 */
#define FEEDBACK_MASK (XIMReverse | XIMUnderline)

static void
add_feedback_attr (PangoAttrList *attrs,
		   const gchar   *str,
		   XIMFeedback    feedback,
		   gint           start_pos,
		   gint           end_pos)
{
  PangoAttribute *attr;
  
  gint start_index = g_utf8_offset_to_pointer (str, start_pos) - str;
  gint end_index = g_utf8_offset_to_pointer (str, end_pos) - str;

  if (feedback & XIMUnderline)
    {
      attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
      attr->start_index = start_index;
      attr->end_index = end_index;

      pango_attr_list_change (attrs, attr);
    }

  if (feedback & XIMReverse)
    {
      attr = pango_attr_foreground_new (0xffff, 0xffff, 0xffff);
      attr->start_index = start_index;
      attr->end_index = end_index;

      pango_attr_list_change (attrs, attr);

      attr = pango_attr_background_new (0, 0, 0);
      attr->start_index = start_index;
      attr->end_index = end_index;

      pango_attr_list_change (attrs, attr);
    }

  if (feedback & ~FEEDBACK_MASK)
    g_warning ("Unrendered feedback style: %#lx", feedback & ~FEEDBACK_MASK);
}

static void     
gtk_im_context_xim_get_preedit_string (GtkIMContext   *context,
				       gchar         **str,
				       PangoAttrList **attrs,
				       gint           *cursor_pos)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (context);
  gchar *utf8 = g_ucs4_to_utf8 (context_xim->preedit_chars, context_xim->preedit_length);

  if (attrs)
    {
      int i;
      XIMFeedback last_feedback = 0;
      gint start = -1;
      
      *attrs = pango_attr_list_new ();

      for (i = 0; i < context_xim->preedit_length; i++)
	{
	  XIMFeedback new_feedback = context_xim->feedbacks[i] & FEEDBACK_MASK;
	  if (new_feedback != last_feedback)
	    {
	      if (start >= 0)
		add_feedback_attr (*attrs, utf8, last_feedback, start, i);
	      
	      last_feedback = new_feedback;
	      start = i;
	    }
	}

      if (start >= 0)
	add_feedback_attr (*attrs, utf8, last_feedback, start, i);
    }

  if (str)
    *str = utf8;
  else
    g_free (utf8);

  if (cursor_pos)
    *cursor_pos = context_xim->preedit_cursor;
}

static void
preedit_start_callback (XIC      xic,
			XPointer client_data,
			XPointer call_data)
{
  GtkIMContext *context = GTK_IM_CONTEXT (client_data);
  
  gtk_signal_emit_by_name (GTK_OBJECT (context), "preedit_start");
  g_print ("Starting preedit!\n");
}		     

static void
preedit_done_callback (XIC      xic,
		     XPointer client_data,
		     XPointer call_data)
{
  GtkIMContext *context = GTK_IM_CONTEXT (client_data);
  
  gtk_signal_emit_by_name (GTK_OBJECT (context), "preedit_end");  
  g_print ("Ending preedit!\n");
}		     

static gint
xim_text_to_utf8 (GtkIMContextXIM *context, XIMText *xim_text, gchar **text)
{
  gint text_length = 0;
  GError *error = NULL;
  gchar *result = NULL;

  if (xim_text && xim_text->string.multi_byte)
    {
      if (xim_text->encoding_is_wchar)
	{
	  g_warning ("Wide character return from Xlib not currently supported");
	  *text = NULL;
	  return 0;
	}

      result = g_convert (xim_text->string.multi_byte,
			  -1,
			  "UTF-8",
			  context->mb_charset,
			  NULL, &text_length,  &error);
      
      if (result)
	{
	  text_length = g_utf8_strlen (result, -1);
	  
	  if (text_length != xim_text->length)
	    {
	      g_warning ("Size mismatch when converting text from input method: supplied length = %d\n, result length = %d", xim_text->length, text_length);
	    }
	}
      else
	{
	  g_warning ("Error converting text from IM to UCS-4: %s", error->message);
	  g_error_free (error);
	}

      *text = result;
      return text_length;
    }
  else
    {
      *text = NULL;
      return 0;
    }
}

static void
preedit_draw_callback (XIC                           xic, 
		       XPointer                      client_data,
		       XIMPreeditDrawCallbackStruct *call_data)
{
  GtkIMContextXIM *context = GTK_IM_CONTEXT_XIM (client_data);

  XIMText *new_xim_text = call_data->text;
  gint new_text_length;
  gunichar *new_text = NULL;
  gint i;
  gint diff;
  gint new_length;
  gchar *tmp;
  
  gint chg_first = CLAMP (call_data->chg_first, 0, context->preedit_length);
  gint chg_length = CLAMP (call_data->chg_length, 0, context->preedit_length - chg_first);

  context->preedit_cursor = call_data->caret;
  
  if (chg_first != call_data->chg_first || chg_length != call_data->chg_length)
    g_warning ("Invalid change to preedit string, first=%d length=%d (orig length == %d)",
	       call_data->chg_first, call_data->chg_length, context->preedit_length);

  new_text_length = xim_text_to_utf8 (context, new_xim_text, &tmp);
  if (tmp)
    {
      new_text = g_utf8_to_ucs4 (tmp, -1);
      g_free (tmp);
    }
  
  diff = new_text_length - chg_length;
  new_length = context->preedit_length + diff;

  if (new_length > context->preedit_size)
    {
      context->preedit_size = new_length;
      context->preedit_chars = g_renew (gunichar, context->preedit_chars, new_length);
      context->feedbacks = g_renew (XIMFeedback, context->feedbacks, new_length);
    }

  if (diff < 0)
    {
      for (i = chg_first + chg_length ; i < context->preedit_length; i++)
	{
	  context->preedit_chars[i + diff] = context->preedit_chars[i];
	  context->feedbacks[i + diff] = context->feedbacks[i];
	}
    }
  else
    {
      for (i = context->preedit_length - 1; i >= chg_first + chg_length ; i--)
	{
	  context->preedit_chars[i + diff] = context->preedit_chars[i];
	  context->feedbacks[i + diff] = context->feedbacks[i];
	}
    }

  for (i = 0; i < new_text_length; i++)
    {
      context->preedit_chars[chg_first + i] = new_text[i];
      context->feedbacks[chg_first + i] = new_xim_text->feedback[i];
    }

  context->preedit_length += diff;

  if (new_text)
    g_free (new_text);

  gtk_signal_emit_by_name (GTK_OBJECT (context), "preedit-changed");
}
    

static void
preedit_caret_callback (XIC                            xic,
			XPointer                       client_data,
			XIMPreeditCaretCallbackStruct *call_data)
{
  GtkIMContextXIM *context = GTK_IM_CONTEXT_XIM (client_data);
  
  if (call_data->direction == XIMAbsolutePosition)
    {
      context->preedit_cursor = call_data->position;
      gtk_signal_emit_by_name (GTK_OBJECT (context), "preedit-changed");
    }
  else
    {
      g_warning ("Caret movement command: %d %d %d not supported",
		 call_data->position, call_data->direction, call_data->style);
    }
}	     

static void
status_start_callback (XIC      xic,
		       XPointer client_data,
		       XPointer call_data)
{
  g_print ("Status start\n");
} 

static void
status_done_callback (XIC      xic,
		      XPointer client_data,
		      XPointer call_data)
{
  g_print ("Status done!\n");
}

static void
status_draw_callback (XIC      xic,
		      XPointer client_data,
		      XIMStatusDrawCallbackStruct *call_data)
{
  GtkIMContextXIM *context = GTK_IM_CONTEXT_XIM (client_data);

  g_print ("Status draw\n");
  if (call_data->type == XIMTextType)
    {
      gchar *text;
      xim_text_to_utf8 (context, call_data->data.text, &text);

      if (text)
	g_print ("  %s\n", text);
    }
  else				/* bitmap */
    {
      g_print ("   bitmap id = %#lx\n", call_data->data.bitmap);
    }
}

static XIC
gtk_im_context_xim_get_ic (GtkIMContextXIM *context_xim)
{
  const char *name1 = NULL;
  XVaNestedList *list1 = NULL;
  const char *name2 = NULL;
  XVaNestedList *list2 = NULL;

  if (!context_xim->ic && context_xim->client_window)
    {
      if ((context_xim->im_info->style & PREEDIT_MASK) == XIMPreeditCallbacks)
	{
	  context_xim->preedit_start_callback.client_data = (XPointer)context_xim;
	  context_xim->preedit_start_callback.callback = (XIMProc)preedit_start_callback;
	  context_xim->preedit_done_callback.client_data = (XPointer)context_xim;
	  context_xim->preedit_done_callback.callback = (XIMProc)preedit_done_callback;
	  context_xim->preedit_draw_callback.client_data = (XPointer)context_xim;
	  context_xim->preedit_draw_callback.callback = (XIMProc)preedit_draw_callback;
	  context_xim->preedit_caret_callback.client_data = (XPointer)context_xim;
	  context_xim->preedit_caret_callback.callback = (XIMProc)preedit_caret_callback;
	  
	  name1 = XNPreeditAttributes;
	  list1 = XVaCreateNestedList (0,
				       XNPreeditStartCallback, &context_xim->preedit_start_callback,
				       XNPreeditDoneCallback, &context_xim->preedit_done_callback,
				       XNPreeditDrawCallback, &context_xim->preedit_draw_callback,
				       XNPreeditCaretCallback, &context_xim->preedit_caret_callback,
				       NULL);
	}

      if ((context_xim->im_info->style & STATUS_MASK) == XIMStatusCallbacks)
	{
	  XVaNestedList *status_attrs;
	  
	  context_xim->status_start_callback.client_data = (XPointer)context_xim;
	  context_xim->status_start_callback.callback = (XIMProc)status_start_callback;
	  context_xim->status_done_callback.client_data = (XPointer)context_xim;
	  context_xim->status_done_callback.callback = (XIMProc)status_done_callback;
	  context_xim->status_draw_callback.client_data = (XPointer)context_xim;
	  context_xim->status_draw_callback.callback = (XIMProc)status_draw_callback;
	  
	  status_attrs = XVaCreateNestedList (0,
					      XNStatusStartCallback, &context_xim->status_start_callback,
					      XNStatusDoneCallback, &context_xim->status_done_callback,
					      XNStatusDrawCallback, &context_xim->status_draw_callback,
					      NULL);
	  
	  if (name1 == NULL)
	    {
	      name1 = XNStatusAttributes;
	      list1 = status_attrs;
	    }
	  else
	    {
	      name2 = XNStatusAttributes;
	      list2 = status_attrs;
	    }
	}

      context_xim->ic = XCreateIC (context_xim->im_info->im,
				   XNInputStyle, context_xim->im_info->style,
				   XNClientWindow, GDK_DRAWABLE_XID (context_xim->client_window),
				   name1, list1,
				   name2, list2,
				   NULL);
      
      if (list1)
	XFree (list1);
      if (list2)
	XFree (list2);
    }

  return context_xim->ic;
}
