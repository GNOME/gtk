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
 *
 * Global clipboard abstraction. 
 */

#include <string.h>

#include "gtkclipboard.h"
#include "gtkinvisible.h"
#include "gtkmain.h"
#include "gtksignal.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#endif

typedef struct _RequestContentsInfo RequestContentsInfo;
typedef struct _RequestTextInfo RequestTextInfo;

struct _GtkClipboard 
{
  GdkAtom selection;

  GtkClipboardGetFunc get_func;
  GtkClipboardClearFunc clear_func;
  gpointer user_data;
  gboolean have_owner;

  guint32 timestamp;

  gboolean have_selection;
};

struct _RequestContentsInfo
{
  GtkClipboardReceivedFunc callback;
  gpointer user_data;
};

struct _RequestTextInfo
{
  GtkClipboardTextReceivedFunc callback;
  gpointer user_data;
};

static void clipboard_unset    (GtkClipboard     *clipboard);
static void selection_received (GtkWidget        *widget,
				GtkSelectionData *selection_data,
				guint             time);

static GSList *clipboards;
static GtkWidget *clipboard_widget;

enum {
  TARGET_STRING,
  TARGET_TEXT,
  TARGET_COMPOUND_TEXT,
  TARGET_UTF8_STRING
};

static const gchar *request_contents_key = "gtk-request-contents";
static GQuark request_contents_key_id = 0;

static const gchar *clipboards_owned_key = "gtk-clipboards-owned";
static GQuark clipboards_owned_key_id = 0;
  

/**
 * gtk_clipboard_get:
 * @selection: a #GdkAtom which identifies the clipboard
 *             to use. A value of %GDK_NONE here is the
 *             same as <literal>gdk_atom_intern ("CLIPBOARD", FALSE)</literal>,
 *             and provides the default clipboard. Another
 *             common value is %GDK_SELECTION_PRIMARY, which
 *             identifies the primary X selection. 
 * 
 * Returns the clipboard object for the given selection.
 * 
 * Return value: the appropriate clipboard object. If no
 *             clipboard already exists, a new one will
 *             be created. Once a clipboard object has
 *             been created, it is persistent for all time.
 **/
GtkClipboard *
gtk_clipboard_get (GdkAtom selection)
{
  GtkClipboard *clipboard = NULL;
  GSList *tmp_list;

  if (selection == GDK_NONE)
    selection = gdk_atom_intern ("CLIPBOARD", FALSE);

  tmp_list = clipboards;
  while (tmp_list)
    {
      clipboard = tmp_list->data;
      if (clipboard->selection == selection)
	break;

      tmp_list = tmp_list->next;
    }

  if (!tmp_list)
    {
      clipboard = g_new0 (GtkClipboard, 1);
      clipboard->selection = selection;
      clipboards = g_slist_prepend (clipboards, clipboard);
    }
  
  return clipboard;
}

static void 
selection_get_cb (GtkWidget          *widget,
		  GtkSelectionData   *selection_data,
		  guint               time,
		  guint               info)
{
  GtkClipboard *clipboard = gtk_clipboard_get (selection_data->selection);

  if (clipboard && clipboard->get_func)
    clipboard->get_func (clipboard, selection_data, info, clipboard->user_data);
}

static gboolean
selection_clear_event_cb (GtkWidget	    *widget,
			  GdkEventSelection *event)
{
  GtkClipboard *clipboard = gtk_clipboard_get (event->selection);

  if (clipboard)
    {
      clipboard_unset (clipboard);
      return TRUE;
    }

  return FALSE;
}

static GtkWidget *
make_clipboard_widget (gboolean provider)
{
  GtkWidget *widget = gtk_invisible_new ();

  gtk_signal_connect (GTK_OBJECT (widget), "selection_received",
		      GTK_SIGNAL_FUNC (selection_received), NULL);

  if (provider)
    {
      /* We need this for gdk_x11_get_server_time() */
      gtk_widget_add_events (widget, GDK_PROPERTY_CHANGE_MASK);
      
      gtk_signal_connect (GTK_OBJECT (widget), "selection_get",
			  GTK_SIGNAL_FUNC (selection_get_cb), NULL);
      gtk_signal_connect (GTK_OBJECT (widget), "selection_clear_event",
			  GTK_SIGNAL_FUNC (selection_clear_event_cb), NULL);
    }

  return widget;
}


static void
ensure_clipboard_widget ()
{
  if (!clipboard_widget)
    clipboard_widget = make_clipboard_widget (TRUE);
}

/* This function makes a very good guess at what the correct
 * timestamp for a selection request should be. If there is
 * a currently processed event, it uses the timestamp for that
 * event, otherwise it uses the current server time. However,
 * if the time resulting from that is older than the time used
 * last time, it uses the time used last time instead.
 *
 * In order implement this correctly, we never use CurrentTime,
 * but actually retrieve the actual timestamp from the server.
 * This is a little slower but allows us to make the guarantee
 * that the times used by this application will always ascend
 * and we won't get selections being rejected just because
 * we are using a correct timestamp from an event, but used
 * CurrentTime previously.
 */
static guint32
clipboard_get_timestamp (GtkClipboard *clipboard)
{
  guint32 timestamp = gtk_get_current_event_time ();

  ensure_clipboard_widget ();
  
  if (timestamp == GDK_CURRENT_TIME)
    {
#ifdef GDK_WINDOWING_X11
      timestamp = gdk_x11_get_server_time (clipboard_widget->window);
#elif defined GDK_WINDOWING_WIN32
      timestamp = GetMessageTime ();
#endif
    }
  else
    {
      if (clipboard->timestamp != GDK_CURRENT_TIME)
	{
	  /* Check to see if clipboard->timestamp is newer than
	   * timestamp, accounting for wraparound.
	   */

	  guint32 max = timestamp + 0x80000000;

	  if ((max > timestamp &&
	       (clipboard->timestamp > timestamp &&
		clipboard->timestamp <= max)) ||
	      (max <= timestamp &&
	       (clipboard->timestamp > timestamp ||
		clipboard->timestamp <= max)))
	    {
	      timestamp = clipboard->timestamp;
	    }
	}
    }

  clipboard->timestamp = timestamp;

  return timestamp;
}

static void
clipboard_owner_destroyed (gpointer data)
{
  GSList *clipboards = data;
  GSList *tmp_list;

  tmp_list = clipboards;
  while (tmp_list)
    {
      GtkClipboard *clipboard = tmp_list->data;

      clipboard->get_func = NULL;
      clipboard->clear_func = NULL;
      clipboard->user_data = NULL;
      clipboard->have_owner = FALSE;

      gtk_clipboard_clear (clipboard);

      tmp_list = tmp_list->next;
    }
  
  g_slist_free (clipboards);
}

static void
clipboard_add_owner_notify (GtkClipboard *clipboard)
{
  if (!clipboards_owned_key_id)
    clipboards_owned_key_id = g_quark_from_static_string (clipboards_owned_key);
  
  if (clipboard->have_owner)
    g_object_set_qdata_full (clipboard->user_data, clipboards_owned_key_id,
			     g_slist_prepend (g_object_steal_qdata (clipboard->user_data,
								    clipboards_owned_key_id),
					      clipboard),
			     clipboard_owner_destroyed);
}

static void
clipboard_remove_owner_notify (GtkClipboard *clipboard)
{
  if (clipboard->have_owner)
     g_object_set_qdata_full (clipboard->user_data, clipboards_owned_key_id,
			      g_slist_remove (g_object_steal_qdata (clipboard->user_data,
								    clipboards_owned_key_id),
					      clipboard),
			      clipboard_owner_destroyed);
}
	  
static gboolean
gtk_clipboard_set_contents (GtkClipboard         *clipboard,
			    const GtkTargetEntry *targets,
			    guint                 n_targets,
			    GtkClipboardGetFunc   get_func,
			    GtkClipboardClearFunc clear_func,
			    gpointer              user_data,
			    gboolean              have_owner)
{
  ensure_clipboard_widget ();

  if (gtk_selection_owner_set (clipboard_widget, clipboard->selection,
			       clipboard_get_timestamp (clipboard)))
    {
      clipboard->have_selection = TRUE;

      if (!(clipboard->have_owner && have_owner) ||
	  clipboard->user_data != user_data)
	{
	  clipboard_unset (clipboard);

	  if (clipboard->get_func)
	    {
	      /* Calling unset() caused the clipboard contents to be reset!
	       * Avoid leaking and return 
	       */
	      if (!(clipboard->have_owner && have_owner) ||
		  clipboard->user_data != user_data)
		{
		  (*clear_func) (clipboard, user_data);
		  return FALSE;
		}
	      else
		return TRUE;
	    }
	  else
	    {
	      clipboard->user_data = user_data;
	      clipboard->have_owner = have_owner;
	      if (have_owner)
		clipboard_add_owner_notify (clipboard);
	    }
	  
 	}

      clipboard->get_func = get_func;
      clipboard->clear_func = clear_func;

      gtk_selection_clear_targets (clipboard_widget, clipboard->selection);
      gtk_selection_add_targets (clipboard_widget, clipboard->selection,
				 targets, n_targets);

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_clipboard_set_with_data:
 * @clipboard:  a #GtkClipboard
 * @targets:    array containing information about the available forms for the
 *              clipboard data
 * @n_targets:  number of elements in @targets
 * @get_func:   function to call to get the actual clipboard data
 * @clear_func: when the clipboard contents are set again, this function will
 *              be called, and @get_func will not be subsequently called.
 * @user_data:  user data to pass to @get_func and @clear_func.
 * 
 * Virtually sets the contents of the specified clipboard by providing
 * a list of supported formats for the clipboard data and a function
 * to call to get the actual data when it is requested.
 * 
 * Return value: %TRUE if setting the clipboard data succeeded. If setting
 *               the clipboard data failed the provided callback functions
 *               will be ignored.
 **/
gboolean
gtk_clipboard_set_with_data (GtkClipboard          *clipboard,
			     const GtkTargetEntry  *targets,
			     guint                  n_targets,
			     GtkClipboardGetFunc    get_func,
			     GtkClipboardClearFunc  clear_func,
			     gpointer               user_data)
{
  g_return_val_if_fail (clipboard != NULL, FALSE);
  g_return_val_if_fail (targets != NULL, FALSE);
  g_return_val_if_fail (get_func != NULL, FALSE);

  return gtk_clipboard_set_contents (clipboard, targets, n_targets,
				     get_func, clear_func, user_data,
				     FALSE);
}

/**
 * gtk_clipboard_set_with_owner:
 * @clipboard:  a #GtkClipboard
 * @targets:    array containing information about the available forms for the
 *              clipboard data
 * @n_targets:  number of elements in @targets
 * @get_func:   function to call to get the actual clipboard data
 * @clear_func: when the clipboard contents are set again, this function will
 *              be called, and @get_func will not be subsequently called.
 * @owner:      an object that "owns" the data. This object will be passed
 *              to the callbacks when called. 
 * 
 * Virtually sets the contents of the specified clipboard by providing
 * a list of supported formats for the clipboard data and a function
 * to call to get the actual data when it is requested.
 *
 * The difference between this function and gtk_clipboard_set_with_data()
 * is that instead of an generic @user_data pointer, a #GObject is passed
 * in. 
 * 
 * Return value: %TRUE if setting the clipboard data succeeded. If setting
 *               the clipboard data failed the provided callback functions
 *               will be ignored.
 **/
gboolean
gtk_clipboard_set_with_owner (GtkClipboard          *clipboard,
			      const GtkTargetEntry  *targets,
			      guint                  n_targets,
			      GtkClipboardGetFunc    get_func,
			      GtkClipboardClearFunc  clear_func,
			      GObject               *owner)
{
  g_return_val_if_fail (clipboard != NULL, FALSE);
  g_return_val_if_fail (targets != NULL, FALSE);
  g_return_val_if_fail (get_func != NULL, FALSE);
  g_return_val_if_fail (G_IS_OBJECT (owner), FALSE);

  return gtk_clipboard_set_contents (clipboard, targets, n_targets,
				     get_func, clear_func, owner,
				     TRUE);
}

/**
 * gtk_clipboard_get_owner:
 * @clipboard: a #GtkClipboard
 * 
 * If the clipboard contents callbacks were set with 
 * gtk_clipboard_set_with_owner(), and the gtk_clipboard_set_with_data() or 
 * gtk_clipboard_clear() has not subsequently called, returns the @owner set 
 * by gtk_clipboard_set_with_owner().
 * 
 * Return value: the owner of the clipboard, if any; otherwise %NULL.
 **/
GObject *
gtk_clipboard_get_owner (GtkClipboard *clipboard)
{
  g_return_val_if_fail (clipboard != NULL, NULL);

  if (clipboard->have_owner)
    return clipboard->user_data;
  else
    return NULL;
}

static void
clipboard_unset (GtkClipboard *clipboard)
{
  GtkClipboardClearFunc old_clear_func;
  gpointer old_data;
  
  old_clear_func = clipboard->clear_func;
  old_data = clipboard->user_data;
	  
  if (clipboard->have_owner)
    {
      clipboard_remove_owner_notify (clipboard);
      clipboard->have_owner = FALSE;
    }
  
  clipboard->get_func = NULL;
  clipboard->clear_func = NULL;
  clipboard->user_data = NULL;
  
  if (old_clear_func)
    old_clear_func (clipboard, old_data);
}

/**
 * gtk_clipboard_clear:
 * @clipboard:  a #GtkClipboard
 * 
 * Clears the contents of the clipboard. Generally this should only
 * be called between the time you call gtk_clipboard_set_contents(),
 * and when the @clear_func you supplied is called. Otherwise, the
 * clipboard may be owned by someone else.
 **/
void
gtk_clipboard_clear (GtkClipboard *clipboard)
{
  g_return_if_fail (clipboard != NULL);

  if (clipboard->have_selection)
    gtk_selection_owner_set (NULL, clipboard->selection,
			     clipboard_get_timestamp (clipboard));
}

static void 
text_get_func (GtkClipboard     *clipboard,
	       GtkSelectionData *selection_data,
	       guint             info,
	       gpointer          data)
{
  gtk_selection_data_set_text (selection_data, data, -1);
}

static void 
text_clear_func (GtkClipboard *clipboard,
		 gpointer      data)
{
  g_free (data);
}

/**
 * gtk_clipboard_set_text:
 * @clipboard: a #GtkClipboard object
 * @text:      a UTF-8 string.
 * @len:       length of @text, in bytes, or -1, in which case
 *             the length will be determined with <function>strlen()</function>.
 * 
 * Sets the contents of the clipboard to the given UTF-8 string. GTK+ will
 * make a copy of the text and take responsibility for responding
 * for requests for the text, and for converting the text into
 * the requested format.
 **/
void 
gtk_clipboard_set_text (GtkClipboard *clipboard,
			const gchar  *text,
			gint          len)
{
  static const GtkTargetEntry targets[] = {
    { "STRING", 0, TARGET_STRING },
    { "TEXT",   0, TARGET_TEXT }, 
    { "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
    { "UTF8_STRING", 0, TARGET_UTF8_STRING }
  };

  g_return_if_fail (clipboard != NULL);
  g_return_if_fail (text != NULL);
  
  if (len < 0)
    len = strlen (text);
  
  gtk_clipboard_set_with_data (clipboard, 
			       targets, G_N_ELEMENTS (targets),
			       text_get_func, text_clear_func,
			       g_strndup (text, len));
}

static void
set_request_contents_info (GtkWidget           *widget,
			   RequestContentsInfo *info)
{
  if (!request_contents_key_id)
    request_contents_key_id = g_quark_from_static_string (request_contents_key);

  gtk_object_set_data_by_id (GTK_OBJECT (widget),
			     request_contents_key_id,
			     info);
}

static RequestContentsInfo *
get_request_contents_info (GtkWidget *widget)
{
  if (!request_contents_key_id)
    return NULL;
  else
    return gtk_object_get_data_by_id (GTK_OBJECT (widget),
				      request_contents_key_id);
}

static void 
selection_received (GtkWidget            *widget,
		    GtkSelectionData     *selection_data,
		    guint                 time)
{
  RequestContentsInfo *request_info = get_request_contents_info (widget);
  set_request_contents_info (widget, NULL);
  
  request_info->callback (gtk_clipboard_get (selection_data->selection), 
			  selection_data,
			  request_info->user_data);

  g_free (request_info);

  if (widget != clipboard_widget)
    gtk_widget_destroy (widget);
}

/**
 * gtk_clipboard_request_contents:
 * @clipboard: a #GtkClipboard
 * @target:    an atom representing the form into which the clipboard
 *             owner should convert the selection.
 * @callback:  A function to call when the results are received
 *             (or the retrieval fails). If the retrieval fails
 *             the length field of @selection_data will be
 *             negative.
 * @user_data: user data to pass to @callback
 * 
 * Requests the contents of clipboard as the given target.
 * When the results of the result are later received the supplied callback
 * will be called.
 **/
void 
gtk_clipboard_request_contents (GtkClipboard            *clipboard,
				GdkAtom                  target,
				GtkClipboardReceivedFunc callback,
				gpointer                 user_data)
{
  RequestContentsInfo *info;
  GtkWidget *widget;

  g_return_if_fail (clipboard != NULL);
  g_return_if_fail (target != GDK_NONE);
  g_return_if_fail (callback != NULL);
  
  ensure_clipboard_widget ();

  if (get_request_contents_info (clipboard_widget))
    widget = make_clipboard_widget (FALSE);
  else
    widget = clipboard_widget;

  info = g_new (RequestContentsInfo, 1);
  info->callback = callback;
  info->user_data = user_data;

  set_request_contents_info (widget, info);

  gtk_selection_convert (widget, clipboard->selection, target,
			 clipboard_get_timestamp (clipboard));
}

static void 
request_text_received_func (GtkClipboard     *clipboard,
			    GtkSelectionData *selection_data,
			    gpointer          data)
{
  RequestTextInfo *info = data;
  gchar *result = NULL;

  result = gtk_selection_data_get_text (selection_data);

  if (!result)
    {
      /* If we asked for UTF8 and didn't get it, try compound_text;
       * if we asked for compound_text and didn't get it, try string;
       * If we asked for anything else and didn't get it, give up.
       */
      if (selection_data->target == gdk_atom_intern ("UTF8_STRING", FALSE))
	{
	  gtk_clipboard_request_contents (clipboard,
					  gdk_atom_intern ("COMPOUND_TEXT", FALSE), 
					  request_text_received_func, info);
	  return;
	}
      else if (selection_data->target == gdk_atom_intern ("COMPOUND_TEXT", FALSE))
	{
	  gtk_clipboard_request_contents (clipboard,
					  GDK_TARGET_STRING, 
					  request_text_received_func, info);
	  return;
	}
    }

  info->callback (clipboard, result, info->user_data);
  g_free (info);
  g_free (result);
}

/**
 * gtk_clipboard_request_text:
 * @clipboard: a #GtkClipboard
 * @callback:  a function to call when the text is received,
 *             or the retrieval fails. (It will always be called
 *             one way or the other.)
 * @user_data: user data to pass to @callback.
 * 
 * Requests the contents of the clipboard as text. When the text is
 * later received, it will be converted to UTF-8 if necessary, and
 * @callback will be called. 
 *
 * The @text parameter to @callback will contain the resulting text if
 * the request succeeded, or %NULL if it failed. This could happen for
 * various reasons, in particular if the clipboard was empty or if the
 * contents of the clipboard could not be converted into text form.
 **/
void 
gtk_clipboard_request_text (GtkClipboard                *clipboard,
			    GtkClipboardTextReceivedFunc callback,
			    gpointer                     user_data)
{
  RequestTextInfo *info;
  
  g_return_if_fail (clipboard != NULL);
  g_return_if_fail (callback != NULL);
  
  info = g_new (RequestTextInfo, 1);
  info->callback = callback;
  info->user_data = user_data;

  gtk_clipboard_request_contents (clipboard, gdk_atom_intern ("UTF8_STRING", FALSE),
				  request_text_received_func,
				  info);
}


typedef struct
{
  GMainLoop *loop;
  gpointer data;
} WaitResults;

static void 
clipboard_received_func (GtkClipboard     *clipboard,
			 GtkSelectionData *selection_data,
			 gpointer          data)
{
  WaitResults *results = data;

  if (selection_data->length >= 0)
    results->data = gtk_selection_data_copy (selection_data);
  
  g_main_quit (results->loop);
}

/**
 * gtk_clipboard_wait_for_contents:
 * @clipboard: a #GtkClipboard
 * @target: an atom representing the form into which the clipboard
 *          owner should convert the selection.
 * 
 * Requests the contents of the clipboard using the given target.
 * This function waits for the data to be received using the main 
 * loop, so events, timeouts, etc, may be dispatched during the wait.
 * 
 * Return value: a newly-allocated #GtkSelectionData object or %NULL
 *               if retrieving the given target failed. If non-%NULL,
 *               this value must be freed with gtk_selection_data_free() 
 *               when you are finished with it.
 **/
GtkSelectionData *
gtk_clipboard_wait_for_contents (GtkClipboard *clipboard,
				 GdkAtom       target)
{
  WaitResults results;

  g_return_val_if_fail (clipboard != NULL, NULL);
  g_return_val_if_fail (target != GDK_NONE, NULL);
  
  results.data = NULL;
  results.loop = g_main_new (TRUE);

  gtk_clipboard_request_contents (clipboard, target, 
				  clipboard_received_func,
				  &results);

  if (g_main_is_running (results.loop))
    {
      GDK_THREADS_LEAVE ();
      g_main_run (results.loop);
      GDK_THREADS_ENTER ();
    }

  g_main_destroy (results.loop);

  return results.data;
}

static void 
clipboard_text_received_func (GtkClipboard *clipboard,
			      const gchar  *text,
			      gpointer      data)
{
  WaitResults *results = data;

  results->data = g_strdup (text);
  g_main_quit (results->loop);
}


/**
 * gtk_clipboard_wait_for_text:
 * @clipboard: a #GtkClipboard
 * 
 * Requests the contents of the clipboard as text and converts
 * the result to UTF-8 if necessary. This function waits for
 * the data to be received using the main loop, so events,
 * timeouts, etc, may be dispatched during the wait.
 * 
 * Return value: a newly-allocated UTF-8 string which must
 *               be freed with g_free(), or %NULL if retrieving
 *               the selection data failed. (This could happen
 *               for various reasons, in particular if the
 *               clipboard was empty or if the contents of the
 *               clipboard could not be converted into text form.)
 **/
gchar *
gtk_clipboard_wait_for_text (GtkClipboard *clipboard)
{
  WaitResults results;

  g_return_val_if_fail (clipboard != NULL, NULL);
  g_return_val_if_fail (clipboard != NULL, NULL);
  
  results.data = NULL;
  results.loop = g_main_new (TRUE);

  gtk_clipboard_request_text (clipboard,
			      clipboard_text_received_func,
			      &results);

  if (g_main_is_running (results.loop))
    {
      GDK_THREADS_LEAVE ();
      g_main_run (results.loop);
      GDK_THREADS_ENTER ();
    }

  g_main_destroy (results.loop);

  return results.data;
}

/**
 * gtk_clipboard_wait_is_text_available:
 * @clipboard: a #GtkClipboard
 * 
 * Test to see if there is text available to be pasted
 * This is done by requesting the TARGETS atom and checking
 * if it contains any of the names: STRING, TEXT, COMPOUND_TEXT,
 * UTF8_STRING. This function waits for the data to be received
 * using the main loop, so events, timeouts, etc, may be dispatched
 * during the wait.
 *
 * This function is a little faster than calling
 * gtk_clipboard_wait_for_text() since it doesn't need to retrieve
 * the actual text.
 * 
 * Return value: %TRUE is there is text available, %FALSE otherwise.
 **/
gboolean
gtk_clipboard_wait_is_text_available (GtkClipboard *clipboard)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  data = gtk_clipboard_wait_for_contents (clipboard, gdk_atom_intern ("TARGETS", FALSE));
  if (data)
    {
      result = gtk_selection_data_targets_include_text (data);
      gtk_selection_data_free (data);
    }

  return result;
}
