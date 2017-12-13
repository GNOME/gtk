/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* This file implements most of the work of the ICCCM selection protocol.
 * The code was written after an intensive study of the equivalent part
 * of John Ousterhout’s Tk toolkit, and does many things in much the 
 * same way.
 *
 * The one thing in the ICCCM that isn’t fully supported here (or in Tk)
 * is side effects targets. For these to be handled properly, MULTIPLE
 * targets need to be done in the order specified. This cannot be
 * guaranteed with the way we do things, since if we are doing INCR
 * transfers, the order will depend on the timing of the requestor.
 *
 * By Owen Taylor <owt1@cornell.edu>	      8/16/97
 */

/* Terminology note: when not otherwise specified, the term "incr" below
 * refers to the _sending_ part of the INCR protocol. The receiving
 * portion is referred to just as “retrieval”. (Terminology borrowed
 * from Tk, because there is no good opposite to “retrieval” in English.
 * “send” can’t be made into a noun gracefully and we’re already using
 * “emission” for something else ....)
 */

/* The MOTIF entry widget seems to ask for the TARGETS target, then
   (regardless of the reply) ask for the TEXT target. It's slightly
   possible though that it somehow thinks we are responding negatively
   to the TARGETS request, though I don't really think so ... */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/**
 * SECTION:gtkselection
 * @Title: Selections
 * @Short_description: Functions for handling inter-process communication
 *     via selections
 * @See_also: #GtkWidget - Much of the operation of selections happens via
 *     signals for #GtkWidget. In particular, if you are using the functions
 *     in this section, you may need to pay attention to
 *     #GtkWidget::selection-get, #GtkWidget::selection-received and
 *     #GtkWidget::selection-clear-event signals
 *
 * The selection mechanism provides the basis for different types
 * of communication between processes. In particular, drag and drop and
 * #GtkClipboard work via selections. You will very seldom or
 * never need to use most of the functions in this section directly;
 * #GtkClipboard provides a nicer interface to the same functionality.
 *
 * Some of the datatypes defined this section are used in
 * the #GtkClipboard and drag-and-drop API’s as well. The
 * #GdkContentFormats object represents
 * lists of data types that are supported when sending or
 * receiving data. The #GtkSelectionData object is used to
 * store a chunk of data along with the data type and other
 * associated information.
 */

#include "config.h"

#include "gtkselection.h"
#include "gtkselectionprivate.h"

#include <stdarg.h>
#include <string.h>
#include "gdk.h"

#include "gtkmain.h"
#include "gtkdebug.h"
#include "gtkintl.h"
#include "gdk-pixbuf/gdk-pixbuf.h"

#include "gdk/gdkcontentformatsprivate.h"
#include "gdk/gdktextureprivate.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#undef DEBUG_SELECTION

/* Maximum size of a sent chunk, in bytes. Also the default size of
   our buffers */
#ifdef GDK_WINDOWING_X11
#define GTK_SELECTION_MAX_SIZE(display)                                 \
  GDK_IS_X11_DISPLAY (display) ?                                        \
  MIN(262144,                                                           \
      XExtendedMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) == 0     \
       ? XMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) - 100         \
       : XExtendedMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) - 100)\
  : G_MAXINT
#else
/* No chunks on Win32 */
#define GTK_SELECTION_MAX_SIZE(display) G_MAXINT
#endif

#define IDLE_ABORT_TIME 30

enum {
  INCR,
  MULTIPLE,
  TARGETS,
  TIMESTAMP,
  SAVE_TARGETS,
  LAST_ATOM
};

typedef struct _GtkIncrConversion GtkIncrConversion;
typedef struct _GtkIncrInfo GtkIncrInfo;
typedef struct _GtkRetrievalInfo GtkRetrievalInfo;

struct _GtkIncrConversion 
{
  GdkAtom	    target;	/* Requested target */
  GdkAtom	    property;	/* Property to store in */
  GtkSelectionData  data;	/* The data being supplied */
  gint		    offset;	/* Current offset in sent selection.
				 *  -1 => All done
				 *  -2 => Only the final (empty) portion
				 *	  left to send */
};

struct _GtkIncrInfo
{
  GdkWindow *requestor;		/* Requestor window - we create a GdkWindow
				   so we can receive events */
  GdkAtom    selection;		/* Selection we're sending */
  
  GtkIncrConversion *conversions; /* Information about requested conversions -
				   * With MULTIPLE requests (benighted 1980's
				   * hardware idea), there can be more than
				   * one */
  gint num_conversions;
  gint num_incrs;		/* number of remaining INCR style transactions */
  guint32 idle_time;
};


struct _GtkRetrievalInfo
{
  GtkWidget *widget;
  GdkAtom selection;		/* Selection being retrieved. */
  GdkAtom target;		/* Form of selection that we requested */
  guint32 idle_time;		/* Number of seconds since we last heard
				   from selection owner */
  guchar   *buffer;		/* Buffer in which to accumulate results */
  gint	   offset;		/* Current offset in buffer, -1 indicates
				   not yet started */
  guint32 notify_time;		/* Timestamp from SelectionNotify */
};

/* Local Functions */
static void gtk_selection_init              (void);
static gboolean gtk_selection_retrieval_timeout (GtkRetrievalInfo *info);
static void gtk_selection_retrieval_report  (GtkRetrievalInfo *info,
					     GdkAtom           type,
					     gint              format,
					     guchar           *buffer,
					     gint              length,
					     guint32           time);
static int  gtk_selection_bytes_per_item    (gint              format);

/* Local Data */
static gint initialize = TRUE;
static GList *current_retrievals = NULL;
static GList *current_incrs = NULL;

static GdkAtom gtk_selection_atoms[LAST_ATOM];

/****************
 * Target Lists *
 ****************/

/*
 * Target lists
 */


static GdkAtom utf8_atom;
static GdkAtom text_atom;
static GdkAtom ctext_atom;
static GdkAtom text_plain_atom;
static GdkAtom text_plain_utf8_atom;
static GdkAtom text_plain_locale_atom;
static GdkAtom text_uri_list_atom;

static void 
init_atoms (void)
{
  gchar *tmp;
  const gchar *charset;

  if (!utf8_atom)
    {
      utf8_atom = gdk_atom_intern_static_string ("UTF8_STRING");
      text_atom = gdk_atom_intern_static_string ("TEXT");
      ctext_atom = gdk_atom_intern_static_string ("COMPOUND_TEXT");
      text_plain_atom = gdk_atom_intern_static_string ("text/plain");
      text_plain_utf8_atom = gdk_atom_intern_static_string ("text/plain;charset=utf-8");
      g_get_charset (&charset);
      tmp = g_strdup_printf ("text/plain;charset=%s", charset);
      text_plain_locale_atom = gdk_atom_intern (tmp, FALSE);
      g_free (tmp);

      text_uri_list_atom = gdk_atom_intern_static_string ("text/uri-list");
    }
}

/**
 * gtk_content_formats_add_text_targets:
 * @list: a #GdkContentFormats
 * 
 * Appends the text targets supported by #GtkSelectionData to
 * the target list. All targets are added with the same @info.
 * 
 * Since: 2.6
 **/
GdkContentFormats *
gtk_content_formats_add_text_targets (GdkContentFormats *list)
{
  GdkContentFormatsBuilder *builder;

  g_return_val_if_fail (list != NULL, NULL);
  
  init_atoms ();

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_formats (builder, list);
  gdk_content_formats_unref (list);

  /* Keep in sync with gtk_selection_data_targets_include_text()
   */
  gdk_content_formats_builder_add_mime_type (builder, utf8_atom);  
  gdk_content_formats_builder_add_mime_type (builder, ctext_atom);  
  gdk_content_formats_builder_add_mime_type (builder, text_atom);  
  gdk_content_formats_builder_add_mime_type (builder, GDK_TARGET_STRING);  
  gdk_content_formats_builder_add_mime_type (builder, text_plain_utf8_atom);  
  if (!g_get_charset (NULL))
    gdk_content_formats_builder_add_mime_type (builder, text_plain_locale_atom);  
  gdk_content_formats_builder_add_mime_type (builder, text_plain_atom);  

  return gdk_content_formats_builder_free (builder);
}

/**
 * gtk_content_formats_add_image_targets:
 * @list: a #GdkContentFormats
 * @writable: whether to add only targets for which GTK+ knows
 *   how to convert a pixbuf into the format
 * 
 * Appends the image targets supported by #GtkSelectionData to
 * the target list. All targets are added with the same @info.
 * 
 * Since: 2.6
 **/
GdkContentFormats *
gtk_content_formats_add_image_targets (GdkContentFormats *list,
		                       gboolean           writable)
{
  GdkContentFormatsBuilder *builder;
  GSList *formats, *f;
  gchar **mimes, **m;

  g_return_val_if_fail (list != NULL, NULL);

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_formats (builder, list);
  gdk_content_formats_unref (list);

  formats = gdk_pixbuf_get_formats ();

  /* Make sure png comes first */
  for (f = formats; f; f = f->next)
    {
      GdkPixbufFormat *fmt = f->data;
      gchar *name; 
 
      name = gdk_pixbuf_format_get_name (fmt);
      if (strcmp (name, "png") == 0)
	{
	  formats = g_slist_delete_link (formats, f);
	  formats = g_slist_prepend (formats, fmt);

	  g_free (name);

	  break;
	}

      g_free (name);
    }  

  for (f = formats; f; f = f->next)
    {
      GdkPixbufFormat *fmt = f->data;

      if (writable && !gdk_pixbuf_format_is_writable (fmt))
	continue;
      
      mimes = gdk_pixbuf_format_get_mime_types (fmt);
      for (m = mimes; *m; m++)
	{
	  gdk_content_formats_builder_add_mime_type (builder, *m);  
	}
      g_strfreev (mimes);
    }

  g_slist_free (formats);

  return gdk_content_formats_builder_free (builder);
}

/**
 * gtk_content_formats_add_uri_targets:
 * @list: a #GdkContentFormats
 * 
 * Appends the URI targets supported by #GtkSelectionData to
 * the target list. All targets are added with the same @info.
 * 
 * Since: 2.6
 **/
GdkContentFormats *
gtk_content_formats_add_uri_targets (GdkContentFormats *list)
{
  GdkContentFormatsBuilder *builder;

  g_return_val_if_fail (list != NULL, NULL);
  
  init_atoms ();

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_formats (builder, list);
  gdk_content_formats_unref (list);

  gdk_content_formats_builder_add_mime_type (builder, text_uri_list_atom);  

  return gdk_content_formats_builder_free (builder);
}

/**
 * gtk_selection_remove_all:
 * @widget: a #GtkWidget 
 * 
 * Removes all handlers and unsets ownership of all 
 * selections for a widget. Called when widget is being
 * destroyed. This function will not generally be
 * called by applications.
 **/
void
gtk_selection_remove_all (GtkWidget *widget)
{
  GList *tmp_list;
  GList *next;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* Remove pending requests/incrs for this widget */
  
  tmp_list = current_retrievals;
  while (tmp_list)
    {
      next = tmp_list->next;
      if (((GtkRetrievalInfo *)tmp_list->data)->widget == widget)
	{
	  current_retrievals = g_list_remove_link (current_retrievals, 
						   tmp_list);
	  /* structure will be freed in timeout */
	  g_list_free (tmp_list);
	}
      tmp_list = next;
    }
}


/**
 * gtk_selection_convert:
 * @widget: The widget which acts as requestor
 * @selection: Which selection to get
 * @target: Form of information desired (e.g., STRING)
 * @time_: Time of request (usually of triggering event)
       In emergency, you could use #GDK_CURRENT_TIME
 * 
 * Requests the contents of a selection. When received, 
 * a “selection-received” signal will be generated.
 * 
 * Returns: %TRUE if requested succeeded. %FALSE if we could not process
 *          request. (e.g., there was already a request in process for
 *          this widget).
 **/
gboolean
gtk_selection_convert (GtkWidget *widget, 
		       GdkAtom	  selection, 
		       GdkAtom	  target,
		       guint32	  time_)
{
  GtkRetrievalInfo *info;
  GList *tmp_list;
  guint id;
  
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (selection != NULL, FALSE);
  
  if (initialize)
    gtk_selection_init ();
  
  if (!gtk_widget_get_realized (widget))
    gtk_widget_realize (widget);
  
  /* Check to see if there are already any retrievals in progress for
     this widget. If we changed GDK to use the selection for the 
     window property in which to store the retrieved information, then
     we could support multiple retrievals for different selections.
     This might be useful for DND. */
  
  tmp_list = current_retrievals;
  while (tmp_list)
    {
      info = (GtkRetrievalInfo *)tmp_list->data;
      if (info->widget == widget)
	return FALSE;
      tmp_list = tmp_list->next;
    }
  
  info = g_slice_new (GtkRetrievalInfo);
  
  info->widget = widget;
  info->selection = selection;
  info->target = target;
  info->idle_time = 0;
  info->buffer = NULL;
  info->offset = -1;
  
  /* Otherwise, we need to go through X */
  
  current_retrievals = g_list_append (current_retrievals, info);
  gdk_selection_convert (gtk_widget_get_window (widget), selection, target, time_);
  id = gdk_threads_add_timeout (1000,
      (GSourceFunc) gtk_selection_retrieval_timeout, info);
  g_source_set_name_by_id (id, "[gtk+] gtk_selection_retrieval_timeout");
  
  return TRUE;
}

/**
 * gtk_selection_data_get_selection:
 * @selection_data: a pointer to a #GtkSelectionData-struct.
 *
 * Retrieves the selection #GdkAtom of the selection data.
 *
 * Returns: (transfer none): the selection #GdkAtom of the selection data.
 *
 * Since: 2.16
 **/
GdkAtom
gtk_selection_data_get_selection (const GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, 0);

  return selection_data->selection;
}

/**
 * gtk_selection_data_get_target:
 * @selection_data: a pointer to a #GtkSelectionData-struct.
 *
 * Retrieves the target of the selection.
 *
 * Returns: (transfer none): the target of the selection.
 *
 * Since: 2.14
 **/
GdkAtom
gtk_selection_data_get_target (const GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, 0);

  return selection_data->target;
}

/**
 * gtk_selection_data_get_data_type:
 * @selection_data: a pointer to a #GtkSelectionData-struct.
 *
 * Retrieves the data type of the selection.
 *
 * Returns: (transfer none): the data type of the selection.
 *
 * Since: 2.14
 **/
GdkAtom
gtk_selection_data_get_data_type (const GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, 0);

  return selection_data->type;
}

/**
 * gtk_selection_data_get_format:
 * @selection_data: a pointer to a #GtkSelectionData-struct.
 *
 * Retrieves the format of the selection.
 *
 * Returns: the format of the selection.
 *
 * Since: 2.14
 **/
gint
gtk_selection_data_get_format (const GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, 0);

  return selection_data->format;
}

/**
 * gtk_selection_data_get_data: (skip)
 * @selection_data: a pointer to a
 *   #GtkSelectionData-struct.
 *
 * Retrieves the raw data of the selection.
 *
 * Returns: (array) (element-type guint8): the raw data of the selection.
 *
 * Since: 2.14
 **/
const guchar*
gtk_selection_data_get_data (const GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, NULL);

  return selection_data->data;
}

/**
 * gtk_selection_data_get_length:
 * @selection_data: a pointer to a #GtkSelectionData-struct.
 *
 * Retrieves the length of the raw data of the selection.
 *
 * Returns: the length of the data of the selection.
 *
 * Since: 2.14
 */
gint
gtk_selection_data_get_length (const GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, -1);

  return selection_data->length;
}

/**
 * gtk_selection_data_get_data_with_length: (rename-to gtk_selection_data_get_data)
 * @selection_data: a pointer to a #GtkSelectionData-struct.
 * @length: (out): return location for length of the data segment
 *
 * Retrieves the raw data of the selection along with its length.
 *
 * Returns: (array length=length): the raw data of the selection
 *
 * Since: 3.0
 */
const guchar*
gtk_selection_data_get_data_with_length (const GtkSelectionData *selection_data,
                                         gint                   *length)
{
  g_return_val_if_fail (selection_data != NULL, NULL);

  *length = selection_data->length;

  return selection_data->data;
}

/**
 * gtk_selection_data_get_display:
 * @selection_data: a pointer to a #GtkSelectionData-struct.
 *
 * Retrieves the display of the selection.
 *
 * Returns: (transfer none): the display of the selection.
 *
 * Since: 2.14
 **/
GdkDisplay *
gtk_selection_data_get_display (const GtkSelectionData *selection_data)
{
  g_return_val_if_fail (selection_data != NULL, NULL);

  return selection_data->display;
}

/**
 * gtk_selection_data_set:
 * @selection_data: a pointer to a #GtkSelectionData-struct.
 * @type: the type of selection data
 * @format: format (number of bits in a unit)
 * @data: (array length=length): pointer to the data (will be copied)
 * @length: length of the data
 * 
 * Stores new data into a #GtkSelectionData object. Should
 * only be called from a selection handler callback.
 * Zero-terminates the stored data.
 **/
void 
gtk_selection_data_set (GtkSelectionData *selection_data,
			GdkAtom		  type,
			gint		  format,
			const guchar	 *data,
			gint		  length)
{
  g_return_if_fail (selection_data != NULL);

  g_free (selection_data->data);
  
  selection_data->type = type;
  selection_data->format = format;
  
  if (data)
    {
      selection_data->data = g_new (guchar, length+1);
      memcpy (selection_data->data, data, length);
      selection_data->data[length] = 0;
    }
  else
    {
      g_return_if_fail (length <= 0);
      
      if (length < 0)
	selection_data->data = NULL;
      else
	selection_data->data = (guchar *) g_strdup ("");
    }
  
  selection_data->length = length;
}

static gboolean
selection_set_string (GtkSelectionData *selection_data,
		      const gchar      *str,
		      gint              len)
{
  gchar *tmp = g_strndup (str, len);
  gchar *latin1 = gdk_utf8_to_string_target (tmp);
  g_free (tmp);
  
  if (latin1)
    {
      gtk_selection_data_set (selection_data,
			      GDK_SELECTION_TYPE_STRING,
			      8, (guchar *) latin1, strlen (latin1));
      g_free (latin1);
      
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
selection_set_compound_text (GtkSelectionData *selection_data,
			     const gchar      *str,
			     gint              len)
{
  gboolean result = FALSE;

#ifdef GDK_WINDOWING_X11
  gchar *tmp;
  guchar *text;
  GdkAtom encoding;
  gint format;
  gint new_length;

  if (GDK_IS_X11_DISPLAY (selection_data->display))
    {
      tmp = g_strndup (str, len);
      if (gdk_x11_display_utf8_to_compound_text (selection_data->display, tmp,
                                                 &encoding, &format, &text, &new_length))
        {
          gtk_selection_data_set (selection_data, encoding, format, text, new_length);
          gdk_x11_free_compound_text (text);

          result = TRUE;
        }
      g_free (tmp);
    }
#endif

  return result;
}

/* Normalize \r and \n into \r\n
 */
static gchar *
normalize_to_crlf (const gchar *str, 
		   gint         len)
{
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

/* Normalize \r and \r\n into \n
 */
static gchar *
normalize_to_lf (gchar *str, 
		 gint   len)
{
  GString *result = g_string_sized_new (len);
  const gchar *p = str;

  while (1)
    {
      if (*p == '\r')
	{
	  p++;
	  if (*p != '\n')
	    g_string_append_c (result, '\n');
	}

      if (*p == '\0')
	break;

      g_string_append_c (result, *p);
      p++;
    }

  return g_string_free (result, FALSE);  
}

static gboolean
selection_set_text_plain (GtkSelectionData *selection_data,
			  const gchar      *str,
			  gint              len)
{
  const gchar *charset = NULL;
  gchar *result;
  GError *error = NULL;

  result = normalize_to_crlf (str, len);
  if (selection_data->target == text_plain_atom)
    charset = "ASCII";
  else if (selection_data->target == text_plain_locale_atom)
    g_get_charset (&charset);

  if (charset)
    {
      gchar *tmp = result;
      result = g_convert_with_fallback (tmp, -1, 
					charset, "UTF-8", 
					NULL, NULL, NULL, &error);
      g_free (tmp);
    }

  if (!result)
    {
      g_warning ("Error converting from %s to %s: %s",
		 "UTF-8", charset, error->message);
      g_error_free (error);
      
      return FALSE;
    }
  
  gtk_selection_data_set (selection_data,
			  selection_data->target, 
			  8, (guchar *) result, strlen (result));
  g_free (result);
  
  return TRUE;
}

static guchar *
selection_get_text_plain (const GtkSelectionData *selection_data)
{
  const gchar *charset = NULL;
  gchar *str, *result;
  gsize len;
  GError *error = NULL;

  str = g_strdup ((const gchar *) selection_data->data);
  len = selection_data->length;
  
  if (selection_data->type == text_plain_atom)
    charset = "ISO-8859-1";
  else if (selection_data->type == text_plain_locale_atom)
    g_get_charset (&charset);

  if (charset)
    {
      gchar *tmp = str;
      str = g_convert_with_fallback (tmp, len, 
				     "UTF-8", charset,
				     NULL, NULL, &len, &error);
      g_free (tmp);

      if (!str)
	{
	  g_warning ("Error converting from %s to %s: %s",
		     charset, "UTF-8", error->message);
	  g_error_free (error);

	  return NULL;
	}
    }
  else if (!g_utf8_validate (str, -1, NULL))
    {
      g_warning ("Error converting from %s to %s: %s",
		 "text/plain;charset=utf-8", "UTF-8", "invalid UTF-8");
      g_free (str);

      return NULL;
    }

  result = normalize_to_lf (str, len);
  g_free (str);

  return (guchar *) result;
}

/**
 * gtk_selection_data_set_text:
 * @selection_data: a #GtkSelectionData
 * @str: a UTF-8 string
 * @len: the length of @str, or -1 if @str is nul-terminated.
 * 
 * Sets the contents of the selection from a UTF-8 encoded string.
 * The string is converted to the form determined by
 * @selection_data->target.
 * 
 * Returns: %TRUE if the selection was successfully set,
 *   otherwise %FALSE.
 **/
gboolean
gtk_selection_data_set_text (GtkSelectionData     *selection_data,
			     const gchar          *str,
			     gint                  len)
{
  g_return_val_if_fail (selection_data != NULL, FALSE);

  if (len < 0)
    len = strlen (str);
  
  init_atoms ();

  if (selection_data->target == utf8_atom)
    {
      gtk_selection_data_set (selection_data,
			      utf8_atom,
			      8, (guchar *)str, len);
      return TRUE;
    }
  else if (selection_data->target == GDK_TARGET_STRING)
    {
      return selection_set_string (selection_data, str, len);
    }
  else if (selection_data->target == ctext_atom ||
	   selection_data->target == text_atom)
    {
      if (selection_set_compound_text (selection_data, str, len))
	return TRUE;
      else if (selection_data->target == text_atom)
	return selection_set_string (selection_data, str, len);
    }
  else if (selection_data->target == text_plain_atom ||
	   selection_data->target == text_plain_utf8_atom ||
	   selection_data->target == text_plain_locale_atom)
    {
      return selection_set_text_plain (selection_data, str, len);
    }

  return FALSE;
}

/**
 * gtk_selection_data_get_text:
 * @selection_data: a #GtkSelectionData
 * 
 * Gets the contents of the selection data as a UTF-8 string.
 * 
 * Returns: (type utf8) (nullable) (transfer full): if the selection data contained a
 *   recognized text type and it could be converted to UTF-8, a newly
 *   allocated string containing the converted text, otherwise %NULL.
 *   If the result is non-%NULL it must be freed with g_free().
 **/
guchar *
gtk_selection_data_get_text (const GtkSelectionData *selection_data)
{
  guchar *result = NULL;

  g_return_val_if_fail (selection_data != NULL, NULL);

  init_atoms ();
  
  if (selection_data->length >= 0 &&
      (selection_data->type == GDK_TARGET_STRING ||
       selection_data->type == ctext_atom ||
       selection_data->type == utf8_atom))
    {
      gchar **list;
      gint i;
      gint count = gdk_text_property_to_utf8_list_for_display (selection_data->display,
      							       selection_data->type,
						   	       selection_data->format, 
						               selection_data->data,
						               selection_data->length,
						               &list);
      if (count > 0)
	result = (guchar *) list[0];

      for (i = 1; i < count; i++)
	g_free (list[i]);
      g_free (list);
    }
  else if (selection_data->length >= 0 &&
	   (selection_data->type == text_plain_atom ||
	    selection_data->type == text_plain_utf8_atom ||
	    selection_data->type == text_plain_locale_atom))
    {
      result = selection_get_text_plain (selection_data);
    }

  return result;
}

/**
 * gtk_selection_data_set_pixbuf:
 * @selection_data: a #GtkSelectionData
 * @pixbuf: a #GdkPixbuf
 * 
 * Sets the contents of the selection from a #GdkPixbuf
 * The pixbuf is converted to the form determined by
 * @selection_data->target.
 * 
 * Returns: %TRUE if the selection was successfully set,
 *   otherwise %FALSE.
 *
 * Since: 2.6
 **/
gboolean
gtk_selection_data_set_pixbuf (GtkSelectionData *selection_data,
			       GdkPixbuf        *pixbuf)
{
  GSList *formats, *f;
  gchar **mimes, **m;
  GdkAtom atom;
  gboolean result;
  gchar *str, *type;
  gsize len;

  g_return_val_if_fail (selection_data != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), FALSE);

  formats = gdk_pixbuf_get_formats ();

  for (f = formats; f; f = f->next)
    {
      GdkPixbufFormat *fmt = f->data;

      mimes = gdk_pixbuf_format_get_mime_types (fmt);
      for (m = mimes; *m; m++)
	{
	  atom = gdk_atom_intern (*m, FALSE);
	  if (selection_data->target == atom)
	    {
	      str = NULL;
	      type = gdk_pixbuf_format_get_name (fmt);
	      result = gdk_pixbuf_save_to_buffer (pixbuf, &str, &len,
						  type, NULL,
                                                  ((strcmp (type, "png") == 0) ?
                                                   "compression" : NULL), "2",
                                                  NULL);
	      if (result)
		gtk_selection_data_set (selection_data,
					atom, 8, (guchar *)str, len);
	      g_free (type);
	      g_free (str);
	      g_strfreev (mimes);
	      g_slist_free (formats);

	      return result;
	    }
	}

      g_strfreev (mimes);
    }

  g_slist_free (formats);
 
  return FALSE;
}

/**
 * gtk_selection_data_set_surface:
 * @selection_data: a #GtkSelectionData
 * @surface: a cairo image surface
 * 
 * Sets the contents of the selection from a cairo image surface.
 * The surface is converted to the form determined by
 * @selection_data->target.
 * 
 * Returns: %TRUE if the selection was successfully set,
 *   otherwise %FALSE.
 *
 * Since: 3.94
 **/
gboolean
gtk_selection_data_set_surface (GtkSelectionData *selection_data,
			        cairo_surface_t  *surface)
{
  GdkPixbuf *pixbuf;
  gboolean retval;

  pixbuf = gdk_pixbuf_get_from_surface (surface,
                                        0, 0,
                                        cairo_image_surface_get_width (surface),
                                        cairo_image_surface_get_height (surface));
  retval = gtk_selection_data_set_pixbuf (selection_data, pixbuf);
  g_object_unref (pixbuf);

  return retval;
}

/**
 * gtk_selection_data_get_surface:
 * @selection_data: a #GtkSelectionData
 * 
 * Gets the contents of the selection data as a cairo surface.
 * 
 * Returns: (nullable) (transfer full): if the selection data
 *   contained a recognized image type and it could be converted to a
 *   cairo surface, a newly allocated surface is returned, otherwise
 *   %NULL.  If the result is non-%NULL it must be freed with
 *   g_object_unref().
 *
 * Since: 3.94
 **/
cairo_surface_t *
gtk_selection_data_get_surface (GtkSelectionData *selection_data)
{
  GdkPixbuf *pixbuf;
  cairo_surface_t *surface;

  pixbuf = gtk_selection_data_get_pixbuf (selection_data);
  surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, 1, NULL);
  g_object_unref (pixbuf);

  return surface;
}

/**
 * gtk_selection_data_get_pixbuf:
 * @selection_data: a #GtkSelectionData
 * 
 * Gets the contents of the selection data as a #GdkPixbuf.
 * 
 * Returns: (nullable) (transfer full): if the selection data
 *   contained a recognized image type and it could be converted to a
 *   #GdkPixbuf, a newly allocated pixbuf is returned, otherwise
 *   %NULL.  If the result is non-%NULL it must be freed with
 *   g_object_unref().
 *
 * Since: 2.6
 **/
GdkPixbuf *
gtk_selection_data_get_pixbuf (const GtkSelectionData *selection_data)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *result = NULL;

  g_return_val_if_fail (selection_data != NULL, NULL);

  if (selection_data->length > 0)
    {
      loader = gdk_pixbuf_loader_new ();
      
      gdk_pixbuf_loader_write (loader, 
			       selection_data->data,
			       selection_data->length,
			       NULL);
      gdk_pixbuf_loader_close (loader, NULL);
      result = gdk_pixbuf_loader_get_pixbuf (loader);
      
      if (result)
	g_object_ref (result);
      
      g_object_unref (loader);
    }

  return result;
}

/**
 * gtk_selection_data_set_texture:
 * @selection_data: a #GtkSelectionData
 * @texture: a #GdkTexture
 * 
 * Sets the contents of the selection from a #GdkTexture.
 * The surface is converted to the form determined by
 * @selection_data->target.
 * 
 * Returns: %TRUE if the selection was successfully set,
 *   otherwise %FALSE.
 *
 * Since: 3.94
 **/
gboolean
gtk_selection_data_set_texture (GtkSelectionData *selection_data,
                                GdkTexture       *texture)
{
  cairo_surface_t *surface;
  gboolean retval;

  g_return_val_if_fail (selection_data != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), FALSE);

  surface = gdk_texture_download_surface (texture);
  retval = gtk_selection_data_set_surface (selection_data, surface);
  cairo_surface_destroy (surface);

  return retval;
}

/**
 * gtk_selection_data_get_texture:
 * @selection_data: a #GtkSelectionData
 * 
 * Gets the contents of the selection data as a #GdkPixbuf.
 * 
 * Returns: (nullable) (transfer full): if the selection data
 *   contained a recognized image type and it could be converted to a
 *   #GdkTexture, a newly allocated texture is returned, otherwise
 *   %NULL.  If the result is non-%NULL it must be freed with
 *   g_object_unref().
 *
 * Since: 3.94
 **/
GdkTexture *
gtk_selection_data_get_texture (const GtkSelectionData *selection_data)
{
  GdkTexture *texture;
  GdkPixbuf *pixbuf;

  g_return_val_if_fail (selection_data != NULL, NULL);

  pixbuf = gtk_selection_data_get_pixbuf (selection_data);
  if (pixbuf == NULL)
    return NULL;

  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return texture;
}

/**
 * gtk_selection_data_set_uris:
 * @selection_data: a #GtkSelectionData
 * @uris: (array zero-terminated=1): a %NULL-terminated array of
 *     strings holding URIs
 * 
 * Sets the contents of the selection from a list of URIs.
 * The string is converted to the form determined by
 * @selection_data->target.
 * 
 * Returns: %TRUE if the selection was successfully set,
 *   otherwise %FALSE.
 *
 * Since: 2.6
 **/
gboolean
gtk_selection_data_set_uris (GtkSelectionData  *selection_data,
			     gchar            **uris)
{
  g_return_val_if_fail (selection_data != NULL, FALSE);
  g_return_val_if_fail (uris != NULL, FALSE);

  init_atoms ();

  if (selection_data->target == text_uri_list_atom)
    {
      GString *list;
      gint i;
      gchar *result;
      gsize length;
      
      list = g_string_new (NULL);
      for (i = 0; uris[i]; i++)
	{
	  g_string_append (list, uris[i]);
	  g_string_append (list, "\r\n");
	}

      result = g_convert (list->str, list->len,
			  "ASCII", "UTF-8", 
			  NULL, &length, NULL);
      g_string_free (list, TRUE);
      
      if (result)
	{
	  gtk_selection_data_set (selection_data,
				  text_uri_list_atom,
				  8, (guchar *)result, length);
	  
	  g_free (result);

	  return TRUE;
	}
    }

  return FALSE;
}

/**
 * gtk_selection_data_get_uris:
 * @selection_data: a #GtkSelectionData
 * 
 * Gets the contents of the selection data as array of URIs.
 *
 * Returns:  (array zero-terminated=1) (element-type utf8) (transfer full): if
 *   the selection data contains a list of
 *   URIs, a newly allocated %NULL-terminated string array
 *   containing the URIs, otherwise %NULL. If the result is
 *   non-%NULL it must be freed with g_strfreev().
 *
 * Since: 2.6
 **/
gchar **
gtk_selection_data_get_uris (const GtkSelectionData *selection_data)
{
  gchar **result = NULL;

  g_return_val_if_fail (selection_data != NULL, NULL);

  init_atoms ();
  
  if (selection_data->length >= 0 &&
      selection_data->type == text_uri_list_atom)
    {
      gchar **list;
      gint count = gdk_text_property_to_utf8_list_for_display (selection_data->display,
      							       utf8_atom,
						   	       selection_data->format, 
						               selection_data->data,
						               selection_data->length,
						               &list);
      if (count > 0)
	result = g_uri_list_extract_uris (list[0]);
      
      g_strfreev (list);
    }

  return result;
}


/**
 * gtk_selection_data_get_targets:
 * @selection_data: a #GtkSelectionData object
 * @targets: (out) (array length=n_atoms) (transfer container):
 *           location to store an array of targets. The result stored
 *           here must be freed with g_free().
 * @n_atoms: location to store number of items in @targets.
 * 
 * Gets the contents of @selection_data as an array of targets.
 * This can be used to interpret the results of getting
 * the standard TARGETS target that is always supplied for
 * any selection.
 * 
 * Returns: %TRUE if @selection_data contains a valid
 *    array of targets, otherwise %FALSE.
 **/
gboolean
gtk_selection_data_get_targets (const GtkSelectionData  *selection_data,
				GdkAtom                **targets,
				gint                    *n_atoms)
{
  g_return_val_if_fail (selection_data != NULL, FALSE);

  if (selection_data->length >= 0 &&
      selection_data->format == 32 &&
      selection_data->type == GDK_SELECTION_TYPE_ATOM)
    {
      if (targets)
	*targets = g_memdup (selection_data->data, selection_data->length);
      if (n_atoms)
	*n_atoms = selection_data->length / sizeof (GdkAtom);

      return TRUE;
    }
  else
    {
      if (targets)
	*targets = NULL;
      if (n_atoms)
	*n_atoms = -1;

      return FALSE;
    }
}

/**
 * gtk_targets_include_text:
 * @targets: (array length=n_targets): an array of #GdkAtoms
 * @n_targets: the length of @targets
 * 
 * Determines if any of the targets in @targets can be used to
 * provide text.
 * 
 * Returns: %TRUE if @targets include a suitable target for text,
 *   otherwise %FALSE.
 *
 * Since: 2.10
 **/
gboolean 
gtk_targets_include_text (GdkAtom *targets,
                          gint     n_targets)
{
  gint i;
  gboolean result = FALSE;

  g_return_val_if_fail (targets != NULL || n_targets == 0, FALSE);

  /* Keep in sync with gdk_content_formats_add_text_targets()
   */
 
  init_atoms ();
 
  for (i = 0; i < n_targets; i++)
    {
      if (targets[i] == utf8_atom ||
	  targets[i] == text_atom ||
	  targets[i] == GDK_TARGET_STRING ||
	  targets[i] == ctext_atom ||
	  targets[i] == text_plain_atom ||
	  targets[i] == text_plain_utf8_atom ||
	  targets[i] == text_plain_locale_atom)
	{
	  result = TRUE;
	  break;
	}
    }
  
  return result;
}

/**
 * gtk_selection_data_targets_include_text:
 * @selection_data: a #GtkSelectionData object
 * 
 * Given a #GtkSelectionData object holding a list of targets,
 * determines if any of the targets in @targets can be used to
 * provide text.
 * 
 * Returns: %TRUE if @selection_data holds a list of targets,
 *   and a suitable target for text is included, otherwise %FALSE.
 **/
gboolean
gtk_selection_data_targets_include_text (const GtkSelectionData *selection_data)
{
  GdkAtom *targets;
  gint n_targets;
  gboolean result = FALSE;

  g_return_val_if_fail (selection_data != NULL, FALSE);

  init_atoms ();

  if (gtk_selection_data_get_targets (selection_data, &targets, &n_targets))
    {
      result = gtk_targets_include_text (targets, n_targets);
      g_free (targets);
    }

  return result;
}

/**
 * gtk_targets_include_image:
 * @targets: (array length=n_targets): an array of #GdkAtoms
 * @n_targets: the length of @targets
 * @writable: whether to accept only targets for which GTK+ knows
 *   how to convert a pixbuf into the format
 * 
 * Determines if any of the targets in @targets can be used to
 * provide a #GdkPixbuf.
 * 
 * Returns: %TRUE if @targets include a suitable target for images,
 *   otherwise %FALSE.
 *
 * Since: 2.10
 **/
gboolean 
gtk_targets_include_image (GdkAtom *targets,
			   gint     n_targets,
			   gboolean writable)
{
  GdkContentFormats *list;
  gint i;
  gboolean result = FALSE;

  g_return_val_if_fail (targets != NULL || n_targets == 0, FALSE);

  list = gdk_content_formats_new (NULL, 0);
  list = gtk_content_formats_add_image_targets (list, writable);
  for (i = 0; i < n_targets && !result; i++)
    {
      if (gdk_content_formats_contain_mime_type (list, targets[i]))
        {
          result = TRUE;
          break;
	}
    }
  gdk_content_formats_unref (list);

  return result;
}
				    
/**
 * gtk_selection_data_targets_include_image:
 * @selection_data: a #GtkSelectionData object
 * @writable: whether to accept only targets for which GTK+ knows
 *   how to convert a pixbuf into the format
 * 
 * Given a #GtkSelectionData object holding a list of targets,
 * determines if any of the targets in @targets can be used to
 * provide a #GdkPixbuf.
 * 
 * Returns: %TRUE if @selection_data holds a list of targets,
 *   and a suitable target for images is included, otherwise %FALSE.
 *
 * Since: 2.6
 **/
gboolean 
gtk_selection_data_targets_include_image (const GtkSelectionData *selection_data,
					  gboolean                writable)
{
  GdkAtom *targets;
  gint n_targets;
  gboolean result = FALSE;

  g_return_val_if_fail (selection_data != NULL, FALSE);

  init_atoms ();

  if (gtk_selection_data_get_targets (selection_data, &targets, &n_targets))
    {
      result = gtk_targets_include_image (targets, n_targets, writable);
      g_free (targets);
    }

  return result;
}

/**
 * gtk_targets_include_uri:
 * @targets: (array length=n_targets): an array of #GdkAtoms
 * @n_targets: the length of @targets
 * 
 * Determines if any of the targets in @targets can be used to
 * provide an uri list.
 * 
 * Returns: %TRUE if @targets include a suitable target for uri lists,
 *   otherwise %FALSE.
 *
 * Since: 2.10
 **/
gboolean 
gtk_targets_include_uri (GdkAtom *targets,
			 gint     n_targets)
{
  gint i;
  gboolean result = FALSE;

  g_return_val_if_fail (targets != NULL || n_targets == 0, FALSE);

  /* Keep in sync with gdk_content_formats_add_uri_targets()
   */

  init_atoms ();

  for (i = 0; i < n_targets; i++)
    {
      if (targets[i] == text_uri_list_atom)
	{
	  result = TRUE;
	  break;
	}
    }
  
  return result;
}

/**
 * gtk_selection_data_targets_include_uri:
 * @selection_data: a #GtkSelectionData object
 * 
 * Given a #GtkSelectionData object holding a list of targets,
 * determines if any of the targets in @targets can be used to
 * provide a list or URIs.
 * 
 * Returns: %TRUE if @selection_data holds a list of targets,
 *   and a suitable target for URI lists is included, otherwise %FALSE.
 *
 * Since: 2.10
 **/
gboolean
gtk_selection_data_targets_include_uri (const GtkSelectionData *selection_data)
{
  GdkAtom *targets;
  gint n_targets;
  gboolean result = FALSE;

  g_return_val_if_fail (selection_data != NULL, FALSE);

  init_atoms ();

  if (gtk_selection_data_get_targets (selection_data, &targets, &n_targets))
    {
      result = gtk_targets_include_uri (targets, n_targets);
      g_free (targets);
    }

  return result;
}

	  
/*************************************************************
 * gtk_selection_init:
 *     Initialize local variables
 *   arguments:
 *     
 *   results:
 *************************************************************/

static void
gtk_selection_init (void)
{
  gtk_selection_atoms[INCR] = gdk_atom_intern_static_string ("INCR");
  gtk_selection_atoms[MULTIPLE] = gdk_atom_intern_static_string ("MULTIPLE");
  gtk_selection_atoms[TIMESTAMP] = gdk_atom_intern_static_string ("TIMESTAMP");
  gtk_selection_atoms[TARGETS] = gdk_atom_intern_static_string ("TARGETS");
  gtk_selection_atoms[SAVE_TARGETS] = gdk_atom_intern_static_string ("SAVE_TARGETS");

  initialize = FALSE;
}

/*************************************************************
 * _gtk_selection_incr_event:
 *     Called whenever an PropertyNotify event occurs for an 
 *     GdkWindow with user_data == NULL. These will be notifications
 *     that a window we are sending the selection to via the
 *     INCR protocol has deleted a property and is ready for
 *     more data.
 *
 *   arguments:
 *     window:	the requestor window
 *     event:	the property event structure
 *
 *   results:
 *************************************************************/

gboolean
_gtk_selection_incr_event (GdkWindow	   *window,
			   GdkEventProperty *event)
{
  GList *tmp_list;
  GtkIncrInfo *info = NULL;
  gint num_bytes;
  guchar *buffer;
  gulong selection_max_size;
  GdkPropertyState state;
  GdkAtom atom;
  int i;
  GdkDisplay *display;
  
  gdk_event_get_property ((GdkEvent *)event, &atom, &state);
  if (state != GDK_PROPERTY_DELETE)
    return FALSE;
  
#ifdef DEBUG_SELECTION
  g_message ("PropertyDelete, property %ld", atom);
#endif

  display = gdk_window_get_display (window);
  selection_max_size = GTK_SELECTION_MAX_SIZE (display);

  /* Now find the appropriate ongoing INCR */
  tmp_list = current_incrs;
  while (tmp_list)
    {
      info = (GtkIncrInfo *)tmp_list->data;
      if (info->requestor == gdk_event_get_window ((GdkEvent *)event))
	break;
      
      tmp_list = tmp_list->next;
    }
  
  if (tmp_list == NULL)
    return FALSE;
  
  /* Find out which target this is for */
  for (i=0; i<info->num_conversions; i++)
    {
      if (info->conversions[i].property == atom &&
	  info->conversions[i].offset != -1)
	{
	  int bytes_per_item;
	  
	  info->idle_time = 0;
	  
	  if (info->conversions[i].offset == -2) /* only the last 0-length
						    piece*/
	    {
	      num_bytes = 0;
	      buffer = NULL;
	    }
	  else
	    {
	      num_bytes = info->conversions[i].data.length -
		info->conversions[i].offset;
	      buffer = info->conversions[i].data.data + 
		info->conversions[i].offset;
	      
	      if (num_bytes > selection_max_size)
		{
		  num_bytes = selection_max_size;
		  info->conversions[i].offset += selection_max_size;
		}
	      else
		info->conversions[i].offset = -2;
	    }
#ifdef DEBUG_SELECTION
	  g_message ("INCR: put %d bytes (offset = %d) into window 0x%lx , property %ld",
		     num_bytes, info->conversions[i].offset, 
		     GDK_WINDOW_XID(info->requestor), atom);
#endif

	  bytes_per_item = gtk_selection_bytes_per_item (info->conversions[i].data.format);

#ifdef GDK_WINDOWING_X11
          if (GDK_IS_X11_DISPLAY (display))
            gdk_x11_display_error_trap_push (display);
#endif

	  gdk_property_change (info->requestor, atom,
			       info->conversions[i].data.type,
			       info->conversions[i].data.format,
			       GDK_PROP_MODE_REPLACE,
			       buffer,
			       num_bytes / bytes_per_item);

#ifdef GDK_WINDOWING_X11
          if (GDK_IS_X11_DISPLAY (display))
            gdk_x11_display_error_trap_pop_ignored (display);
#endif
	  
	  if (info->conversions[i].offset == -2)
	    {
	      g_free (info->conversions[i].data.data);
	      info->conversions[i].data.data = NULL;
	    }
	  
	  if (num_bytes == 0)
	    {
	      info->num_incrs--;
	      info->conversions[i].offset = -1;
	    }
	}
    }
  
  /* Check if we're finished with all the targets */
  
  if (info->num_incrs == 0)
    {
      current_incrs = g_list_remove_link (current_incrs, tmp_list);
      g_list_free (tmp_list);
      /* Let the timeout free it */
    }
  
  return TRUE;
}

/*************************************************************
 * _gtk_selection_notify:
 *     Handler for “selection-notify-event” signals on windows
 *     where a retrieval is currently in process. The selection
 *     owner has responded to our conversion request.
 *   arguments:
 *     widget:		Widget getting signal
 *     event:		Selection event structure
 *     info:		Information about this retrieval
 *   results:
 *     was event handled?
 *************************************************************/

gboolean
_gtk_selection_notify (GtkWidget	 *widget,
		       GdkEventSelection *event)
{
  GList *tmp_list;
  GtkRetrievalInfo *info = NULL;
  GdkWindow *window;
  guchar  *buffer = NULL;
  gint length;
  GdkAtom type;
  GdkAtom selection;
  GdkAtom property;
  gint	  format;
  guint32 time;

  gdk_event_get_selection ((GdkEvent *)event, &selection);
  gdk_event_get_selection_property ((GdkEvent *)event, &property, NULL, NULL);
  time = gdk_event_get_time ((GdkEvent *)event);

#ifdef DEBUG_SELECTION
  g_message ("Initial receipt of selection %ld, target %ld (property = %ld)",
	     selection, target, property);
#endif

  window = gtk_widget_get_window (widget);

  tmp_list = current_retrievals;
  while (tmp_list)
    {
      info = (GtkRetrievalInfo *)tmp_list->data;
      if (info->widget == widget && info->selection == selection)
	break;
      tmp_list = tmp_list->next;
    }
  
  if (!tmp_list)		/* no retrieval in progress */
    return FALSE;

  if (property != NULL)
    length = gdk_selection_property_get (window, &buffer,
					 &type, &format);
  else
    length = 0; /* silence gcc */
  
  if (property == NULL || buffer == NULL)
    {
      current_retrievals = g_list_remove_link (current_retrievals, tmp_list);
      g_list_free (tmp_list);
      /* structure will be freed in timeout */
      gtk_selection_retrieval_report (info, NULL, 0, NULL, -1, time);
      
      return TRUE;
    }
  
  if (type == gtk_selection_atoms[INCR])
    {
      /* The remainder of the selection will come through PropertyNotify
	 events */

      info->notify_time = time;
      info->idle_time = 0;
      info->offset = 0;		/* Mark as OK to proceed */
      gdk_window_set_events (window,
                             gdk_window_get_events (window)
			     | GDK_PROPERTY_CHANGE_MASK);
    }
  else
    {
      /* We don't delete the info structure - that will happen in timeout */
      current_retrievals = g_list_remove_link (current_retrievals, tmp_list);
      g_list_free (tmp_list);
      
      info->offset = length;
      gtk_selection_retrieval_report (info,
				      type, format, 
				      buffer, length, time);
    }

  gdk_property_delete (window, property);

  g_free (buffer);
  
  return TRUE;
}

/*************************************************************
 * _gtk_selection_property_notify:
 *     Handler for “property-notify-event” signals on windows
 *     where a retrieval is currently in process. The selection
 *     owner has added more data.
 *   arguments:
 *     widget:		Widget getting signal
 *     event:		Property event structure
 *     info:		Information about this retrieval
 *   results:
 *     was event handled?
 *************************************************************/

gboolean
_gtk_selection_property_notify (GtkWidget	*widget,
				GdkEventProperty *event)
{
  GList *tmp_list;
  GtkRetrievalInfo *info = NULL;
  GdkWindow *window;
  guchar *new_buffer;
  int length;
  GdkAtom type;
  gint	  format;
  GdkAtom property;
  GdkPropertyState state;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  gdk_event_get_property ((GdkEvent *)event, &property, &state);

#if defined(GDK_WINDOWING_WIN32) || defined(GDK_WINDOWING_X11)
  if ((state != GDK_PROPERTY_NEW_VALUE) ||  /* property was deleted */
      (property != gdk_atom_intern_static_string ("GDK_SELECTION"))) /* not the right property */
#endif
    return FALSE;
  
#ifdef DEBUG_SELECTION
  g_message ("PropertyNewValue, property %ld", property);
#endif
  
  tmp_list = current_retrievals;
  while (tmp_list)
    {
      info = (GtkRetrievalInfo *)tmp_list->data;
      if (info->widget == widget)
	break;
      tmp_list = tmp_list->next;
    }
  
  if (!tmp_list)		/* No retrieval in progress */
    return FALSE;
  
  if (info->offset < 0)		/* We haven't got the SelectionNotify
				   for this retrieval yet */
    return FALSE;
  
  info->idle_time = 0;

  window = gtk_widget_get_window (widget);
  length = gdk_selection_property_get (window, &new_buffer,
				       &type, &format);
  gdk_property_delete (window, property);

  /* We could do a lot better efficiency-wise by paying attention to
     what length was sent in the initial INCR transaction, instead of
     doing memory allocation at every step. But its only guaranteed to
     be a _lower bound_ (pretty useless!) */
  
  if (length == 0 || type == NULL)		/* final zero length portion */
    {
      /* Info structure will be freed in timeout */
      current_retrievals = g_list_remove_link (current_retrievals, tmp_list);
      g_list_free (tmp_list);
      gtk_selection_retrieval_report (info,
				      type, format, 
				      (type == NULL) ?  NULL : info->buffer,
				      (type == NULL) ?  -1 : info->offset,
				      info->notify_time);
    }
  else				/* append on newly arrived data */
    {
      if (!info->buffer)
	{
#ifdef DEBUG_SELECTION
	  g_message ("Start - Adding %d bytes at offset 0",
		     length);
#endif
	  info->buffer = new_buffer;
	  info->offset = length;
	}
      else
	{
	  
#ifdef DEBUG_SELECTION
	  g_message ("Appending %d bytes at offset %d",
		     length,info->offset);
#endif
	  /* We copy length+1 bytes to preserve guaranteed null termination */
	  info->buffer = g_realloc (info->buffer, info->offset+length+1);
	  memcpy (info->buffer + info->offset, new_buffer, length+1);
	  info->offset += length;
	  g_free (new_buffer);
	}
    }
  
  return TRUE;
}

/*************************************************************
 * gtk_selection_retrieval_timeout:
 *     Timeout callback while receiving a selection.
 *   arguments:
 *     info:	Information about this retrieval
 *   results:
 *************************************************************/

static gboolean
gtk_selection_retrieval_timeout (GtkRetrievalInfo *info)
{
  GList *tmp_list;
  gboolean retval;

  /* Determine if retrieval has finished by checking if it still in
     list of pending retrievals */
  
  tmp_list = current_retrievals;
  while (tmp_list)
    {
      if (info == (GtkRetrievalInfo *)tmp_list->data)
	break;
      tmp_list = tmp_list->next;
    }
  
  /* If retrieval is finished */
  if (!tmp_list || info->idle_time >= IDLE_ABORT_TIME)
    {
      if (tmp_list && info->idle_time >= IDLE_ABORT_TIME)
	{
	  current_retrievals = g_list_remove_link (current_retrievals, tmp_list);
	  g_list_free (tmp_list);
	  gtk_selection_retrieval_report (info, NULL, 0, NULL, -1, GDK_CURRENT_TIME);
	}
      
      g_free (info->buffer);
      g_slice_free (GtkRetrievalInfo, info);
      
      retval =  FALSE;		/* remove timeout */
    }
  else
    {
      info->idle_time++;
      
      retval =  TRUE;		/* timeout will happen again */
    }

  return retval;
}

/*************************************************************
 * gtk_selection_retrieval_report:
 *     Emits a “selection-received” signal.
 *   arguments:
 *     info:	  information about the retrieval that completed
 *     buffer:	  buffer containing data (NULL => errror)
 *     time:      timestamp for data in buffer
 *   results:
 *************************************************************/

static void
gtk_selection_retrieval_report (GtkRetrievalInfo *info,
				GdkAtom type, gint format, 
				guchar *buffer, gint length,
				guint32 time)
{
  GtkSelectionData data;
  
  data.selection = info->selection;
  data.target = info->target;
  data.type = type;
  data.format = format;
  
  data.length = length;
  data.data = buffer;
  data.display = gtk_widget_get_display (info->widget);
  
  g_signal_emit_by_name (info->widget,
			 "selection-received", 
			 &data, time);
}

/**
 * gtk_selection_data_copy:
 * @data: a pointer to a #GtkSelectionData-struct.
 * 
 * Makes a copy of a #GtkSelectionData-struct and its data.
 * 
 * Returns: a pointer to a copy of @data.
 **/
GtkSelectionData*
gtk_selection_data_copy (const GtkSelectionData *data)
{
  GtkSelectionData *new_data;
  
  g_return_val_if_fail (data != NULL, NULL);
  
  new_data = g_slice_new (GtkSelectionData);
  *new_data = *data;

  if (data->data)
    {
      new_data->data = g_malloc (data->length + 1);
      memcpy (new_data->data, data->data, data->length + 1);
    }
  
  return new_data;
}

/**
 * gtk_selection_data_free:
 * @data: a pointer to a #GtkSelectionData-struct.
 * 
 * Frees a #GtkSelectionData-struct returned from
 * gtk_selection_data_copy().
 **/
void
gtk_selection_data_free (GtkSelectionData *data)
{
  g_return_if_fail (data != NULL);
  
  g_free (data->data);
  
  g_slice_free (GtkSelectionData, data);
}

G_DEFINE_BOXED_TYPE (GtkSelectionData, gtk_selection_data,
                     gtk_selection_data_copy,
                     gtk_selection_data_free)

static int
gtk_selection_bytes_per_item (gint format)
{
  switch (format)
    {
    case 8:
      return sizeof (char);
      break;
    case 16:
      return sizeof (short);
      break;
    case 32:
      return sizeof (long);
      break;
    default:
      g_assert_not_reached();
    }
  return 0;
}
