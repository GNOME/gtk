/* GTK - The GIMP Toolkit
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include "gtkbutton.h"
#include "gtkhscrollbar.h"
#include "gtkhseparator.h"
#include "gtkmain.h"
#include "gtkpreview.h"
#include "gtkrc.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtktable.h"
#include "gtktext.h"
#include "gtkvbox.h"
#include "gtkvscrollbar.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtkprivate.h"


/* Private type definitions
 */
typedef struct _GtkInitFunction	         GtkInitFunction;
typedef struct _GtkTimeoutFunction       GtkTimeoutFunction;
typedef struct _GtkIdleFunction	         GtkIdleFunction;
typedef struct _GtkInputFunction         GtkInputFunction;
typedef struct _GtkKeySnooperData        GtkKeySnooperData;

struct _GtkInitFunction
{
  GtkFunction function;
  gpointer data;
};

struct _GtkTimeoutFunction
{
  gint tag;
  guint32 start;
  guint32 interval;
  guint32 originterval;
  gint interp;
  GtkFunction function;
  gpointer data;
  GtkDestroyNotify destroy;
};

struct _GtkIdleFunction
{
  gint tag;
  gint interp;
  GtkFunction function;
  gpointer data;
  GtkDestroyNotify destroy;
};

struct _GtkInputFunction
{
  GtkCallbackMarshal callback;
  gpointer data;
  GtkDestroyNotify destroy;
};

struct _GtkKeySnooperData
{
  GtkKeySnoopFunc func;
  gpointer func_data;
  gint id;
};

static void  gtk_exit_func		 (void);
static void  gtk_timeout_insert		 (GtkTimeoutFunction *timeoutf);
static void  gtk_handle_current_timeouts (guint32	      the_time);
static void  gtk_handle_current_idles	 (void);
static gint  gtk_invoke_key_snoopers     (GtkWidget          *grab_widget,
					  GdkEvent           *event);
static void  gtk_handle_timeouts	 (void);
static void  gtk_handle_idle		 (void);
static void  gtk_handle_timer		 (void);
static void  gtk_propagate_event	 (GtkWidget	     *widget,
					  GdkEvent	     *event);
static void  gtk_error			 (gchar		     *str);
static void  gtk_warning		 (gchar		     *str);
static void  gtk_message		 (gchar		     *str);
static void  gtk_print			 (gchar		     *str);


static gint done;
static guint main_level = 0;
static gint initialized = FALSE;
static GdkEvent *next_event = NULL;
static GList *current_events = NULL;

static GSList *grabs = NULL;		   /* A stack of unique grabs. The grabbing
					    *  widget is the first one on the list.
					    */
static GList *init_functions = NULL;	   /* A list of init functions.
					    */
static GList *timeout_functions = NULL;	   /* A list of timeout functions sorted by
					    *  when the length of the time interval
					    *  remaining. Therefore, the first timeout
					    *  function to expire is at the head of
					    *  the list and the last to expire is at
					    *  the tail of the list.
					    */
static GList *idle_functions = NULL;	   /* A list of idle functions.
					    */

static GList *current_idles = NULL;
static GList *current_timeouts = NULL;
static GMemChunk *timeout_mem_chunk = NULL;
static GMemChunk *idle_mem_chunk = NULL;

static GSList *key_snoopers = NULL;

static GdkVisual *gtk_visual;		   /* The visual to be used in creating new
					    *  widgets.
					    */
static GdkColormap *gtk_colormap;	   /* The colormap to be used in creating new
					    *  widgets.
					    */


void
gtk_init (int	 *argc,
	  char ***argv)
{
  if (0)
    {
      g_set_error_handler (gtk_error);
      g_set_warning_handler (gtk_warning);
      g_set_message_handler (gtk_message);
      g_set_print_handler (gtk_print);
    }
  
  /* Initialize "gdk". We simply pass along the 'argc' and 'argv'
   *  parameters as they contain information that
   */
  gdk_init (argc, argv);
  
  /* Initialize the default visual and colormap to be
   *  used in creating widgets. (We want to use the system
   *  defaults so as to be nice to the colormap).
   */
  gtk_visual = gdk_visual_get_system ();
  gtk_colormap = gdk_colormap_get_system ();
  gtk_rc_init ();
  
  gtk_type_init ();
  
  /* Register an exit function to make sure we are able to cleanup.
   */
  if (ATEXIT (gtk_exit_func))
    g_warning ("unable to register exit function");
  
  /* Set the 'initialized' flag.
   */
  initialized = TRUE;
}

void
gtk_exit (int errorcode)
{
  /* Only if "gtk" has been initialized should we de-initialize.
   */
  /* de-initialisation is done by the gtk_exit_funct(),
   * no need to do this here (Alex J.)
   */
  gdk_exit(errorcode);
}

gchar*
gtk_set_locale ()
{
  return gdk_set_locale ();
}

void
gtk_main ()
{
  GList *tmp_list;
  GList *functions;
  GtkInitFunction *init;
  int old_done;
  
  main_level++;
  
  tmp_list = functions = init_functions;
  init_functions = NULL;
  
  while (tmp_list)
    {
      init = tmp_list->data;
      tmp_list = tmp_list->next;
      
      (* init->function) (init->data);
      g_free (init);
    }
  
  g_list_free (functions);
  
  old_done = done;
  while (!gtk_main_iteration ())
    ;
  done = old_done;
  
  main_level--;
}

guint
gtk_main_level (void)
{
  return main_level;
}

void
gtk_main_quit ()
{
  done = TRUE;
}

gint
gtk_events_pending (void)
{
  return gdk_events_pending() + (next_event != NULL) ? 1 : 0;
}

gint 
gtk_main_iteration ()
{
  return gtk_main_iteration_do (TRUE);
}

gint
gtk_main_iteration_do (gboolean blocking)
{
  GtkWidget *event_widget;
  GtkWidget *grab_widget;
  GdkEvent *event = NULL;
  GList *tmp_list;
  
  done = FALSE;
  
  /* If this is a recursive call, and there are pending timeouts or
   * idles, finish them, then return immediately to avoid blocking
   * in gdk_event_get()
   */
  if (current_timeouts)
    {
      gtk_handle_current_timeouts( gdk_time_get());
      return done;
    }
  if (current_idles)
    {
      gtk_handle_current_idles();
      return done;
    }
  
  /* If there is a valid event in 'next_event' then move it to 'event'
   */
  if (next_event)
    {
      event = next_event;
      next_event = NULL;
    }
  
  /* If we don't have an event then get one.
   */
  if (!event)
    {
      /* Handle setting of the "gdk" timer. If there are no
       *  timeout functions, then the timer is turned off.
       *  If there are timeout functions, then the timer is
       *  set to the shortest timeout interval (which is
       *  the first timeout function).
       */
      gtk_handle_timer ();
      
      if (blocking) event = gdk_event_get ();
    }
  
  /* "gdk_event_get" can return FALSE if the timer goes off
   *  and no events are pending. Therefore, we should make
   *  sure that we got an event before continuing.
   */
  if (event)
    {
      /* If there are any events pending then get the next one.
       */
      if (gdk_events_pending () > 0)
	next_event = gdk_event_get ();
      
      /* Try to compress enter/leave notify events. These event
       *  pairs occur when the mouse is dragged quickly across
       *  a window with many buttons (or through a menu). Instead
       *  of highlighting and de-highlighting each widget that
       *  is crossed it is better to simply de-highlight the widget
       *  which contained the mouse initially and highlight the
       *  widget which ends up containing the mouse.
       */
      if (next_event)
	if (((event->type == GDK_ENTER_NOTIFY) ||
	     (event->type == GDK_LEAVE_NOTIFY)) &&
	    ((next_event->type == GDK_ENTER_NOTIFY) ||
	     (next_event->type == GDK_LEAVE_NOTIFY)) &&
	    (next_event->type != event->type) &&
	    (next_event->any.window == event->any.window))
	  {
	    gdk_event_free (event);
	    gdk_event_free (next_event);
	    next_event = NULL;

	    return done;
	  }

      /* Push the event onto a stack of current events for
       * gtk_current_event_get().
       */
      current_events = g_list_prepend (current_events, event);
      
      /* Find the widget which got the event. We store the widget
       *  in the user_data field of GdkWindow's.
       */
      event_widget = gtk_get_event_widget (event);
      
      /* If there is a grab in effect...
       */
      if (grabs)
	{
	  grab_widget = grabs->data;
	  
	  /* If the grab widget is an ancestor of the event widget
	   *  then we send the event to the original event widget.
	   *  This is the key to implementing modality.
	   */
	  if (gtk_widget_is_ancestor (event_widget, grab_widget))
	    grab_widget = event_widget;
	}
      else
	{
	  grab_widget = event_widget;
	}
      
      /* Not all events get sent to the grabbing widget.
       * The delete, destroy, expose, focus change and resize
       *  events still get sent to the event widget because
       *  1) these events have no meaning for the grabbing widget
       *  and 2) redirecting these events to the grabbing widget
       *  could cause the display to be messed up.
       */
      switch (event->type)
	{
	case GDK_NOTHING:
	  break;
	  
	case GDK_DELETE:
 	  gtk_widget_ref (event_widget);
	  if (gtk_widget_event (event_widget, event))
	    gtk_widget_destroy (event_widget);
 	  gtk_widget_unref (event_widget);
	  break;
	  
	case GDK_DESTROY:
 	  gtk_widget_ref (event_widget);
	  gtk_widget_event (event_widget, event);
	  gtk_widget_destroy (event_widget);
 	  gtk_widget_unref (event_widget);
	  break;
	  
	case GDK_PROPERTY_NOTIFY:
	  /* To handle selection INCR transactions, we select
	   * PropertyNotify events on the requestor window and create
	   * a corresponding (fake) GdkWindow so that events get
	   * here. There won't be a widget though, so we have to handle
	   * them specially
	   */
	  
	  if (event_widget == NULL)
	    {
	      gtk_selection_incr_event (event->any.window,
					&event->property);
	      break;
	    }
	  /* otherwise fall through */
	case GDK_EXPOSE:
	case GDK_NO_EXPOSE:
	case GDK_FOCUS_CHANGE:
	case GDK_CONFIGURE:
	case GDK_MAP:
	case GDK_UNMAP:
	case GDK_SELECTION_CLEAR:
	case GDK_SELECTION_REQUEST:
	case GDK_SELECTION_NOTIFY:
	case GDK_CLIENT_EVENT:
	case GDK_DRAG_BEGIN:
	case GDK_DRAG_REQUEST:
	case GDK_DROP_ENTER:
	case GDK_DROP_LEAVE:
	case GDK_DROP_DATA_AVAIL:
	case GDK_VISIBILITY_NOTIFY:
	  gtk_widget_event (event_widget, event);
	  break;
	  
	case GDK_KEY_PRESS:
	case GDK_KEY_RELEASE:
	  if (key_snoopers)
	    {
	      if (gtk_invoke_key_snoopers (grab_widget, event))
		break;
	    }
	  /* else fall through */
	case GDK_MOTION_NOTIFY:
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
	case GDK_PROXIMITY_IN:
	case GDK_PROXIMITY_OUT:
	case GDK_OTHER_EVENT:
	  gtk_propagate_event (grab_widget, event);
	  break;
	  
	case GDK_ENTER_NOTIFY:
	  if (grab_widget && GTK_WIDGET_IS_SENSITIVE (grab_widget))
	    {
	      gtk_widget_event (grab_widget, event);
	      if (event_widget == grab_widget)
		GTK_PRIVATE_SET_FLAG (event_widget, GTK_LEAVE_PENDING);
	    }
	  break;

	case GDK_LEAVE_NOTIFY:
	  if (event_widget && GTK_WIDGET_LEAVE_PENDING (event_widget))
	    {
	      GTK_PRIVATE_UNSET_FLAG (event_widget, GTK_LEAVE_PENDING);
	      gtk_widget_event (event_widget, event);
	    }
	  else if (grab_widget && GTK_WIDGET_IS_SENSITIVE (grab_widget))
	    gtk_widget_event (grab_widget, event);
	  break;
	}
      
      tmp_list = current_events;
      current_events = g_list_remove_link (current_events, tmp_list);
      g_list_free_1 (tmp_list);

      gdk_event_free (event);
    }
  else
    {
      if (gdk_events_pending() == 0)
	gtk_handle_idle ();
    }
  
  /* Handle a timeout functions that may have expired.
   */
  gtk_handle_timeouts ();
  
  return done;
}

gint
gtk_true (void)
{
  return TRUE;
}

gint
gtk_false (void)
{
  return FALSE;
}

void
gtk_grab_add (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);

  if (!GTK_WIDGET_HAS_GRAB (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_GRAB);
      
      grabs = g_slist_prepend (grabs, widget);
      gtk_widget_ref (widget);
    }
}

void
gtk_grab_remove (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);

  if (GTK_WIDGET_HAS_GRAB (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_GRAB);

      grabs = g_slist_remove (grabs, widget);
      gtk_widget_unref (widget);
    }
}

void
gtk_init_add (GtkFunction function,
	      gpointer	  data)
{
  GtkInitFunction *init;
  
  init = g_new (GtkInitFunction, 1);
  init->function = function;
  init->data = data;
  
  init_functions = g_list_prepend (init_functions, init);
}

gint
gtk_key_snooper_install (GtkKeySnoopFunc snooper,
			 gpointer        func_data)
{
  GtkKeySnooperData *data;
  static guint snooper_id = 1;

  g_return_val_if_fail (snooper != NULL, 0);

  data = g_new (GtkKeySnooperData, 1);
  data->func = snooper;
  data->func_data = func_data;
  data->id = snooper_id++;
  key_snoopers = g_slist_prepend (key_snoopers, data);

  return data->id;
}

void
gtk_key_snooper_remove (gint            snooper_id)
{
  GtkKeySnooperData *data = NULL;
  GSList *slist;

  slist = key_snoopers;
  while (slist)
    {
      data = slist->data;
      if (data->id == snooper_id)
	break;

      slist = slist->next;
      data = NULL;
    }
  if (data)
    key_snoopers = g_slist_remove (key_snoopers, data);
}

static gint
gtk_invoke_key_snoopers (GtkWidget *grab_widget,
			 GdkEvent  *event)
{
  GSList *slist;
  gint return_val = FALSE;

  slist = key_snoopers;
  while (slist && !return_val)
    {
      GtkKeySnooperData *data;

      data = slist->data;
      slist = slist->next;
      return_val = (*data->func) (grab_widget, (GdkEventKey*) event, data->func_data);
    }

  return return_val;
}

static gint
gtk_timeout_add_internal (guint32     interval,
			  gint	      interp,
			  GtkFunction function,
			  gpointer    data,
			  GtkDestroyNotify destroy)
{
  static gint timeout_tag = 1;
  GtkTimeoutFunction *timeoutf;
  
  /* Create a new timeout function structure.
   * The start time is the current time.
   */
  if (!timeout_mem_chunk)
    timeout_mem_chunk = g_mem_chunk_new ("timeout mem chunk", sizeof (GtkTimeoutFunction),
					 1024, G_ALLOC_AND_FREE);
  
  timeoutf = g_chunk_new (GtkTimeoutFunction, timeout_mem_chunk);
  
  timeoutf->tag = timeout_tag++;
  timeoutf->start = gdk_time_get ();
  timeoutf->interval = interval;
  timeoutf->originterval = interval;
  timeoutf->interp = interp;
  timeoutf->function = function;
  timeoutf->data = data;
  timeoutf->destroy = destroy;
  
  gtk_timeout_insert (timeoutf);
  
  return timeoutf->tag;
}

static void
gtk_timeout_destroy (GtkTimeoutFunction *timeoutf)
{
  if (timeoutf->destroy)
    (timeoutf->destroy) (timeoutf->data);
  g_mem_chunk_free (timeout_mem_chunk, timeoutf);
}

gint
gtk_timeout_add (guint32     interval,
		 GtkFunction function,
		 gpointer    data)
{
  return gtk_timeout_add_internal (interval, FALSE, function, data, NULL);
}

gint
gtk_timeout_add_interp (guint32		   interval,
			GtkCallbackMarshal function,
			gpointer	   data,
			GtkDestroyNotify   destroy)
{
  return gtk_timeout_add_internal (interval, TRUE,
				   (GtkFunction) function,
				   data, destroy);
}

void
gtk_timeout_remove (gint tag)
{
  GtkTimeoutFunction *timeoutf;
  GList *tmp_list;
  
  /* Remove a timeout function.
   * (Which, basically, involves searching the
   *  list for the tag).
   */
  tmp_list = timeout_functions;
  while (tmp_list)
    {
      timeoutf = tmp_list->data;
      
      if (timeoutf->tag == tag)
	{
	  timeout_functions = g_list_remove_link (timeout_functions, tmp_list);
	  g_list_free (tmp_list);
	  gtk_timeout_destroy (timeoutf);
	  
	  return;
	}
      
      tmp_list = tmp_list->next;
    }
  
  tmp_list = current_timeouts;
  while (tmp_list)
    {
      timeoutf = tmp_list->data;
      
      if (timeoutf->tag == tag)
	{
	  current_timeouts = g_list_remove_link (current_timeouts, tmp_list);
	  g_list_free (tmp_list);
	  gtk_timeout_destroy (timeoutf);
	  
	  return;
	}
      
      tmp_list = tmp_list->next;
    }
}

static gint
gtk_idle_add_internal (gint		interp,
		       GtkFunction	function,
		       gpointer		data,
		       GtkDestroyNotify destroy)
{
  static gint idle_tag = 1;
  GtkIdleFunction *idlef;
  
  if (!idle_mem_chunk)
    idle_mem_chunk = g_mem_chunk_new ("idle mem chunk", sizeof (GtkIdleFunction),
				      1024, G_ALLOC_AND_FREE);
  
  idlef = g_chunk_new (GtkIdleFunction, idle_mem_chunk);
  
  idlef->tag = idle_tag++;
  idlef->interp = interp;
  idlef->function = function;
  idlef->data = data;
  idlef->destroy = destroy;
  
  idle_functions = g_list_append (idle_functions, idlef);
  
  return idlef->tag;
}

static void
gtk_idle_destroy (GtkIdleFunction *idlef)
{
  if (idlef->destroy)
    idlef->destroy (idlef->data);
  g_mem_chunk_free (idle_mem_chunk, idlef);
}

gint
gtk_idle_add (GtkFunction function,
	      gpointer	  data)
{
  return gtk_idle_add_internal (FALSE, function, data, NULL);
}

gint
gtk_idle_add_interp (GtkCallbackMarshal function,
		     gpointer		data,
		     GtkDestroyNotify	destroy)
{
  return gtk_idle_add_internal (TRUE, (GtkFunction)function, data, destroy);
}

void
gtk_idle_remove (gint tag)
{
  GtkIdleFunction *idlef;
  GList *tmp_list;
  
  tmp_list = idle_functions;
  while (tmp_list)
    {
      idlef = tmp_list->data;
      
      if (idlef->tag == tag)
	{
	  idle_functions = g_list_remove_link (idle_functions, tmp_list);
	  g_list_free (tmp_list);
	  gtk_idle_destroy (idlef);
	  
	  return;
	}
      
      tmp_list = tmp_list->next;
    }
  
  tmp_list = current_idles;
  while (tmp_list)
    {
      idlef = tmp_list->data;
      
      if (idlef->tag == tag)
	{
	  current_idles = g_list_remove_link (current_idles, tmp_list);
	  g_list_free (tmp_list);
	  gtk_idle_destroy (idlef);
	  
	  return;
	}
      
      tmp_list = tmp_list->next;
    }
}

void
gtk_idle_remove_by_data (gpointer data)
{
  GtkIdleFunction *idlef;
  GList *tmp_list;
  
  tmp_list = idle_functions;
  while (tmp_list)
    {
      idlef = tmp_list->data;
      
      if (idlef->data == data)
	{
	  idle_functions = g_list_remove_link (idle_functions, tmp_list);
	  g_list_free (tmp_list);
	  g_mem_chunk_free (idle_mem_chunk, idlef);
	  
	  return;
	}
      
      tmp_list = tmp_list->next;
    }
  
  tmp_list = current_idles;
  while (tmp_list)
    {
      idlef = tmp_list->data;
      
      if (idlef->data == data)
	{
	  current_idles = g_list_remove_link (current_idles, tmp_list);
	  g_list_free (tmp_list);
	  g_mem_chunk_free (idle_mem_chunk, idlef);
	  
	  return;
	}
      
      tmp_list = tmp_list->next;
    }
}

static void
gtk_invoke_input_function (GtkInputFunction *input,
			   gint source,
			   GdkInputCondition condition)
{
  GtkArg args[3];
  args[0].type = GTK_TYPE_INT;
  args[0].name = NULL;
  GTK_VALUE_INT(args[0]) = source;
  args[1].type = GTK_TYPE_GDK_INPUT_CONDITION;
  args[1].name = NULL;
  GTK_VALUE_FLAGS(args[1]) = condition;
  args[2].type = GTK_TYPE_NONE;
  args[2].name = NULL;

  input->callback (NULL, input->data, 2, args);
}

static void
gtk_destroy_input_function (GtkInputFunction *input)
{
  if (input->destroy)
    (input->destroy) (input->data);
  g_free (input);
}

gint
gtk_input_add_interp (gint source,
		      GdkInputCondition condition,
		      GtkCallbackMarshal callback,
		      gpointer data,
		      GtkDestroyNotify destroy)
{
  GtkInputFunction *input = g_new (GtkInputFunction, 1);
  input->callback = callback;
  input->data = data;
  input->destroy = destroy;
  return gdk_input_add_interp (source,
			       condition,
			       (GdkInputFunction) gtk_invoke_input_function,
			       input,
			       (GdkDestroyNotify) gtk_destroy_input_function);
}

void
gtk_input_remove (gint tag)
{
  gdk_input_remove (tag);
}

GdkEvent *
gtk_get_current_event ()
{
  if (current_events)
    return gdk_event_copy ((GdkEvent *) current_events->data);
  else
    return NULL;
}

GtkWidget*
gtk_get_event_widget (GdkEvent *event)
{
  GtkWidget *widget;

  widget = NULL;
  if (event->any.window)
    gdk_window_get_user_data (event->any.window, (void**) &widget);
  
  return widget;
}

static void
gtk_exit_func ()
{
  if (initialized)
    {
      initialized = FALSE;
      gtk_preview_uninit ();
    }
}

static void
gtk_timeout_insert (GtkTimeoutFunction *timeoutf)
{
  GtkTimeoutFunction *temp;
  GList *temp_list;
  GList *new_list;
  
  g_assert (timeoutf != NULL);
  
  /* Insert the timeout function appropriately.
   * Appropriately meaning sort it into the list
   *  of timeout functions.
   */
  temp_list = timeout_functions;
  while (temp_list)
    {
      temp = temp_list->data;
      if (timeoutf->interval < temp->interval)
	{
	  new_list = g_list_alloc ();
	  new_list->data = timeoutf;
	  new_list->next = temp_list;
	  new_list->prev = temp_list->prev;
	  if (temp_list->prev)
	    temp_list->prev->next = new_list;
	  temp_list->prev = new_list;
	  
	  if (temp_list == timeout_functions)
	    timeout_functions = new_list;
	  
	  return;
	}
      
      temp_list = temp_list->next;
    }
  
  timeout_functions = g_list_append (timeout_functions, timeoutf);
}

static gint
gtk_invoke_timeout_function (GtkTimeoutFunction *timeoutf)
{
  if (!timeoutf->interp)
    return timeoutf->function (timeoutf->data);
  else
    {
      GtkArg args[1];
      gint ret_val = FALSE;
      args[0].name = NULL;
      args[0].type = GTK_TYPE_BOOL;
      args[0].d.pointer_data = &ret_val;
      ((GtkCallbackMarshal)timeoutf->function) (NULL,
						timeoutf->data,
						0, args);
      return ret_val;
    }
}

static void
gtk_handle_current_timeouts (guint32 the_time)
{
  GList *tmp_list;
  GtkTimeoutFunction *timeoutf;
  
  while (current_timeouts)
    {
      tmp_list = current_timeouts;
      timeoutf = tmp_list->data;
      
      current_timeouts = g_list_remove_link (current_timeouts, tmp_list);
      g_list_free (tmp_list);
      
      if (gtk_invoke_timeout_function (timeoutf) == FALSE)
	{
	  gtk_timeout_destroy (timeoutf);
	}
      else
	{
	  timeoutf->interval = timeoutf->originterval;
	  timeoutf->start = the_time;
	  gtk_timeout_insert (timeoutf);
	}
    }
}

static void
gtk_handle_timeouts ()
{
  guint32 the_time;
  GList *tmp_list;
  GList *tmp_list2;
  GtkTimeoutFunction *timeoutf;
  
  /* Caller must already have called gtk_handle_current_timeouts if
   * necessary */
  g_assert (current_timeouts == NULL);
  
  if (timeout_functions)
    {
      the_time = gdk_time_get ();
      
      tmp_list = timeout_functions;
      while (tmp_list)
	{
	  timeoutf = tmp_list->data;
	  
	  if (timeoutf->interval <= (the_time - timeoutf->start))
	    {
	      tmp_list2 = tmp_list;
	      tmp_list = tmp_list->next;
	      
	      timeout_functions = g_list_remove_link (timeout_functions, tmp_list2);
	      current_timeouts = g_list_concat (current_timeouts, tmp_list2);
	    }
	  else
	    {
	      timeoutf->interval -= (the_time - timeoutf->start);
	      timeoutf->start = the_time;
	      tmp_list = tmp_list->next;
	    }
	}
      
      if (current_timeouts)
	gtk_handle_current_timeouts(the_time);
    }
}

static gint
gtk_idle_invoke_function (GtkIdleFunction *idlef)
{
  if (!idlef->interp)
    return idlef->function (idlef->data);
  else
    {
      GtkArg args[1];
      gint ret_val = FALSE;
      args[0].name = NULL;
      args[0].type = GTK_TYPE_BOOL;
      args[0].d.pointer_data = &ret_val;
      ((GtkCallbackMarshal)idlef->function) (NULL,
					     idlef->data,
					     0, args);
      return ret_val;
    }
}

static void
gtk_handle_current_idles ()
{
  GList *tmp_list;
  GtkIdleFunction *idlef;
  
  while (current_idles)
    {
      tmp_list = current_idles;
      idlef = tmp_list->data;
      
      current_idles = g_list_remove_link (current_idles, tmp_list);
      
      if (gtk_idle_invoke_function (idlef) == FALSE)
	{
	  g_list_free (tmp_list);
	  gtk_idle_destroy (idlef);
	}
      else
	{
	  idle_functions = g_list_concat (idle_functions, tmp_list);
	}
    }
}

static void
gtk_handle_idle ()
{
  /* Caller must already have called gtk_handle_current_idles if
   * necessary
   */
  g_assert (current_idles == NULL);
  
  if (idle_functions)
    {
      current_idles = idle_functions;
      idle_functions = NULL;
      
      gtk_handle_current_idles();
    }
}

static void
gtk_handle_timer ()
{
  GtkTimeoutFunction *timeoutf;
  
  if (idle_functions)
    {
      gdk_timer_set (0);
      gdk_timer_enable ();
    }
  else if (timeout_functions)
    {
      timeoutf = timeout_functions->data;
      gdk_timer_set (timeoutf->interval);
      gdk_timer_enable ();
    }
  else
    {
      gdk_timer_set (0);
      gdk_timer_disable ();
    }
}

static void
gtk_propagate_event (GtkWidget *widget,
		     GdkEvent  *event)
{
  GtkWidget *parent;
  GtkWidget *tmp;
  gint handled_event;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (event != NULL);
  
  handled_event = FALSE;
  gtk_widget_ref (widget);

  if ((event->type == GDK_KEY_PRESS) ||
      (event->type == GDK_KEY_RELEASE))
    {

      /* Only send key events to window widgets.
       *  The window widget will in turn pass the
       *  key event on to the currently focused widget
       *  for that window.
       */
      parent = gtk_widget_get_ancestor (widget, gtk_window_get_type ());
      handled_event = (parent &&
		       GTK_WIDGET_IS_SENSITIVE (parent) &&
		       gtk_widget_event (parent, event));
    }
  
  /* Other events get propagated up the widget tree
   *  so that parents can see the button and motion
   *  events of the children.
   */
  tmp = widget;
  while (!handled_event && tmp)
    {
      gtk_widget_ref (tmp);
      handled_event = (GTK_OBJECT_DESTROYED (tmp) ||
		       !GTK_WIDGET_IS_SENSITIVE (tmp) ||
		       gtk_widget_event (tmp, event));
      parent = tmp->parent;
      gtk_widget_unref (tmp);
      tmp = parent;
    }

  gtk_widget_unref (widget);
}


static void
gtk_error (gchar *str)
{
  gtk_print (str);
}

static void
gtk_warning (gchar *str)
{
  gtk_print (str);
}

static void
gtk_message (gchar *str)
{
  gtk_print (str);
}

static void
gtk_print (gchar *str)
{
  static GtkWidget *window = NULL;
  static GtkWidget *text;
  static int level = 0;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *table;
  GtkWidget *hscrollbar;
  GtkWidget *vscrollbar;
  GtkWidget *separator;
  GtkWidget *button;
  
  if (level > 0)
    {
      fputs (str, stdout);
      fflush (stdout);
      return;
    }
  
  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  (GtkSignalFunc) gtk_widget_destroyed,
			  &window);
      
      gtk_window_set_title (GTK_WINDOW (window), "Messages");
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);
      
      
      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);
      
      
      table = gtk_table_new (2, 2, FALSE);
      gtk_table_set_row_spacing (GTK_TABLE (table), 0, 2);
      gtk_table_set_col_spacing (GTK_TABLE (table), 0, 2);
      gtk_box_pack_start (GTK_BOX (box2), table, TRUE, TRUE, 0);
      gtk_widget_show (table);
      
      text = gtk_text_new (NULL, NULL);
      gtk_text_set_editable (GTK_TEXT (text), FALSE);
      gtk_table_attach_defaults (GTK_TABLE (table), text, 0, 1, 0, 1);
      gtk_widget_show (text);
      gtk_widget_realize (text);
      
      hscrollbar = gtk_hscrollbar_new (GTK_TEXT (text)->hadj);
      gtk_table_attach (GTK_TABLE (table), hscrollbar, 0, 1, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (hscrollbar);
      
      vscrollbar = gtk_vscrollbar_new (GTK_TEXT (text)->vadj);
      gtk_table_attach (GTK_TABLE (table), vscrollbar, 1, 2, 0, 1,
			GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (vscrollbar);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);
      
      
      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);
      
      
      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 (GtkSignalFunc) gtk_widget_hide,
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }
  
  level += 1;
  gtk_text_insert (GTK_TEXT (text), NULL, NULL, NULL, str, -1);
  level -= 1;
  
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
}
