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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* This file implements most of the work of the ICCM selection protocol.
 * The code was written after an intensive study of the equivalent part
 * of John Ousterhout's Tk toolkit, and does many things in much the 
 * same way.
 *
 * The one thing in the ICCM that isn't fully supported here (or in Tk)
 * is side effects targets. For these to be handled properly, MULTIPLE
 * targets need to be done in the order specified. This cannot be
 * guaranteed with the way we do things, since if we are doing INCR
 * transfers, the order will depend on the timing of the requestor.
 *
 * By Owen Taylor <owt1@cornell.edu>	      8/16/97
 */

/* Terminology note: when not otherwise specified, the term "incr" below
 * refers to the _sending_ part of the INCR protocol. The receiving
 * portion is referred to just as "retrieval". (Terminology borrowed
 * from Tk, because there is no good opposite to "retrieval" in English.
 * "send" can't be made into a noun gracefully and we're already using
 * "emission" for something else ....)
 */

/* The MOTIF entry widget seems to ask for the TARGETS target, then
   (regardless of the reply) ask for the TEXT target. It's slightly
   possible though that it somehow thinks we are responding negatively
   to the TARGETS request, though I don't really think so ... */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdarg.h>
#include <string.h>
#include <gdk/gdkx.h>
/* we need this for gdk_window_lookup() */
#include "gtkmain.h"
#include "gtkselection.h"
#include "gtksignal.h"

/* #define DEBUG_SELECTION */

/* Maximum size of a sent chunk, in bytes. Also the default size of
   our buffers */
#define GTK_SELECTION_MAX_SIZE 4000

enum {
  INCR,
  MULTIPLE,
  TARGETS,
  TIMESTAMP,
  LAST_ATOM
};

typedef struct _GtkSelectionInfo GtkSelectionInfo;
typedef struct _GtkIncrConversion GtkIncrConversion;
typedef struct _GtkIncrInfo GtkIncrInfo;
typedef struct _GtkRetrievalInfo GtkRetrievalInfo;

struct _GtkSelectionInfo
{
  GdkAtom    selection;
  GtkWidget *widget;		/* widget that owns selection */
  guint32    time;		/* time used to acquire selection */
};

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
  GtkWidget *widget;		/* Selection owner */
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
static gint gtk_selection_incr_timeout      (GtkIncrInfo      *info);
static gint gtk_selection_retrieval_timeout (GtkRetrievalInfo *info);
static void gtk_selection_retrieval_report  (GtkRetrievalInfo *info,
					     GdkAtom           type,
					     gint              format,
					     guchar           *buffer,
					     gint              length,
					     guint32           time);
static void gtk_selection_invoke_handler    (GtkWidget        *widget,
					     GtkSelectionData *data,
					     guint             time);
static void gtk_selection_default_handler   (GtkWidget        *widget,
					     GtkSelectionData *data);
static int  gtk_selection_bytes_per_item    (gint              format);

/* Local Data */
static gint initialize = TRUE;
static GList *current_retrievals = NULL;
static GList *current_incrs = NULL;
static GList *current_selections = NULL;

static GdkAtom gtk_selection_atoms[LAST_ATOM];
static const char *gtk_selection_handler_key = "gtk-selection-handlers";

/****************
 * Target Lists *
 ****************/

/*
 * Target lists
 */

GtkTargetList *
gtk_target_list_new (const GtkTargetEntry *targets,
		     guint                 ntargets)
{
  GtkTargetList *result = g_new (GtkTargetList, 1);
  result->list = NULL;
  result->ref_count = 1;

  if (targets)
    gtk_target_list_add_table (result, targets, ntargets);
  
  return result;
}

void               
gtk_target_list_ref (GtkTargetList *list)
{
  g_return_if_fail (list != NULL);

  list->ref_count++;
}

void               
gtk_target_list_unref (GtkTargetList *list)
{
  g_return_if_fail (list != NULL);
  g_return_if_fail (list->ref_count > 0);

  list->ref_count--;
  if (list->ref_count == 0)
    {
      GList *tmp_list = list->list;
      while (tmp_list)
	{
	  GtkTargetPair *pair = tmp_list->data;
	  g_free (pair);

	  tmp_list = tmp_list->next;
	}
      
      g_list_free (list->list);
      g_free (list);
    }
}

void 
gtk_target_list_add (GtkTargetList *list,
		     GdkAtom            target,
		     guint              flags,
		     guint              info)
{
  GtkTargetPair *pair;

  g_return_if_fail (list != NULL);
  
  pair = g_new (GtkTargetPair, 1);
  pair->target = target;
  pair->flags = flags;
  pair->info = info;

  list->list = g_list_append (list->list, pair);
}

void               
gtk_target_list_add_table (GtkTargetList        *list,
			   const GtkTargetEntry *targets,
			   guint                 ntargets)
{
  gint i;

  for (i=ntargets-1; i >= 0; i--)
    {
      GtkTargetPair *pair = g_new (GtkTargetPair, 1);
      pair->target = gdk_atom_intern (targets[i].target, FALSE);
      pair->flags = targets[i].flags;
      pair->info = targets[i].info;
      
      list->list = g_list_prepend (list->list, pair);
    }
}

void 
gtk_target_list_remove (GtkTargetList *list,
			GdkAtom            target)
{
  GList *tmp_list;

  g_return_if_fail (list != NULL);

  tmp_list = list->list;
  while (tmp_list)
    {
      GtkTargetPair *pair = tmp_list->data;
      
      if (pair->target == target)
	{
	  g_free (pair);

	  list->list = g_list_remove_link (list->list, tmp_list);
	  g_list_free_1 (tmp_list);

	  return;
	}
      
      tmp_list = tmp_list->next;
    }
}

gboolean
gtk_target_list_find (GtkTargetList *list,
		      GdkAtom        target,
		      guint         *info)
{
  GList *tmp_list = list->list;
  while (tmp_list)
    {
      GtkTargetPair *pair = tmp_list->data;

      if (pair->target == target)
	{
	  *info = pair->info;
	  return TRUE;
	}
      tmp_list = tmp_list->next;
    }

  return FALSE;
}


/*************************************************************
 * gtk_selection_owner_set:
 *     Claim ownership of a selection.
 *   arguments:
 *     widget:		new selection owner
 *     selection:	which selection
 *     time:		time (use GDK_CURRENT_TIME only if necessary)
 *
 *   results:
 *************************************************************/

gint
gtk_selection_owner_set (GtkWidget *widget,
			 GdkAtom    selection,
			 guint32    time)
{
  GList *tmp_list;
  GtkWidget *old_owner;
  GtkSelectionInfo *selection_info = NULL;
  GdkWindow *window;
  
  if (widget == NULL)
    window = NULL;
  else
    {
      if (!GTK_WIDGET_REALIZED (widget))
	gtk_widget_realize (widget);
      
      window = widget->window;
    }
  
  tmp_list = current_selections;
  while (tmp_list)
    {
      selection_info = (GtkSelectionInfo *)tmp_list->data;
      
      if (selection_info->selection == selection)
	break;
      
      tmp_list = tmp_list->next;
    }
  
  if (tmp_list == NULL)
    selection_info = NULL;
  else
    if (selection_info->widget == widget)
      return TRUE;
  
  if (gdk_selection_owner_set (window, selection, time, TRUE))
    {
      old_owner = NULL;
      
      if (widget == NULL)
	{
	  if (selection_info)
	    {
	      old_owner = selection_info->widget;
	      current_selections = g_list_remove_link (current_selections,
						       tmp_list);
	      g_list_free (tmp_list);
	      g_free (selection_info);
	    }
	}
      else
	{
	  if (selection_info == NULL)
	    {
	      selection_info = g_new (GtkSelectionInfo, 1);
	      selection_info->selection = selection;
	      selection_info->widget = widget;
	      selection_info->time = time;
	      current_selections = g_list_append (current_selections, 
						  selection_info);
	    }
	  else
	    {
	      old_owner = selection_info->widget;
	      selection_info->widget = widget;
	      selection_info->time = time;
	    }
	}
      /* If another widget in the application lost the selection,
       *  send it a GDK_SELECTION_CLEAR event, unless we're setting
       *  the owner to None, in which case an event will be sent */
      if (old_owner && (widget != NULL))
	{
	  GdkEventSelection event;
	  
	  event.type = GDK_SELECTION_CLEAR;
	  event.window = old_owner->window;
	  event.selection = selection;
	  event.time = time;
	  
	  gtk_widget_event (old_owner, (GdkEvent *) &event);
	}
      return TRUE;
    }
  else
    return FALSE;
}

/*************************************************************
 * gtk_selection_add_target
 *     Add specified target to list of supported targets
 *
 *   arguments:
 *     widget:	   The widget for which this target applies
 *     selection:
 *     target:
 *     info:       guint to pass to to the selection_get signal 
 *
 *   results:
 *************************************************************/

typedef struct _GtkSelectionTargetList GtkSelectionTargetList;

struct _GtkSelectionTargetList {
  GdkAtom selection;
  GtkTargetList *list;
};

static GtkTargetList *
gtk_selection_target_list_get (GtkWidget    *widget,
			       GdkAtom       selection)
{
  GtkSelectionTargetList *sellist;
  GList *tmp_list;
  GList *lists;

  lists = gtk_object_get_data (GTK_OBJECT (widget), gtk_selection_handler_key);
  
  tmp_list = lists;
  while (tmp_list)
    {
      sellist = tmp_list->data;
      if (sellist->selection == selection)
	return sellist->list;
      tmp_list = tmp_list->next;
    }

  sellist = g_new (GtkSelectionTargetList, 1);
  sellist->selection = selection;
  sellist->list = gtk_target_list_new (NULL, 0);

  lists = g_list_prepend (lists, sellist);
  gtk_object_set_data (GTK_OBJECT (widget), gtk_selection_handler_key, lists);

  return sellist->list;
}

static void
gtk_selection_target_list_remove (GtkWidget    *widget)
{
  GtkSelectionTargetList *sellist;
  GList *tmp_list;
  GList *lists;

  lists = gtk_object_get_data (GTK_OBJECT (widget), gtk_selection_handler_key);
  
  tmp_list = lists;
  while (tmp_list)
    {
      sellist = tmp_list->data;

      gtk_target_list_unref (sellist->list);

      g_free (sellist);
      tmp_list = tmp_list->next;
    }

  g_list_free (lists);
  gtk_object_set_data (GTK_OBJECT (widget), gtk_selection_handler_key, NULL);
}

void 
gtk_selection_add_target (GtkWidget	    *widget, 
			  GdkAtom	     selection,
			  GdkAtom	     target,
			  guint              info)
{
  GtkTargetList *list;

  g_return_if_fail (widget != NULL);

  list = gtk_selection_target_list_get (widget, selection);
  gtk_target_list_add (list, target, 0, info);
}

void 
gtk_selection_add_targets (GtkWidget            *widget, 
			   GdkAtom               selection,
			   const GtkTargetEntry *targets,
			   guint                 ntargets)
{
  GtkTargetList *list;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (targets != NULL);
  
  list = gtk_selection_target_list_get (widget, selection);
  gtk_target_list_add_table (list, targets, ntargets);
}

/*************************************************************
 * gtk_selection_remove_all:
 *     Removes all handlers and unsets ownership of all 
 *     selections for a widget. Called when widget is being
 *     destroyed
 *     
 *   arguments:
 *     widget:	  The widget
 *   results:
 *************************************************************/

void
gtk_selection_remove_all (GtkWidget *widget)
{
  GList *tmp_list;
  GList *next;
  GtkSelectionInfo *selection_info;
  
  /* Remove pending requests/incrs for this widget */
  
  tmp_list = current_incrs;
  while (tmp_list)
    {
      next = tmp_list->next;
      if (((GtkIncrInfo *)tmp_list->data)->widget == widget)
	{
	  current_incrs = g_list_remove_link (current_incrs, tmp_list);
	  /* structure will be freed in timeout */
	  g_list_free (tmp_list);
	}
      tmp_list = next;
    }
  
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
  
  /* Disclaim ownership of any selections */
  
  tmp_list = current_selections;
  while (tmp_list)
    {
      next = tmp_list->next;
      selection_info = (GtkSelectionInfo *)tmp_list->data;
      
      if (selection_info->widget == widget)
	{	
	  gdk_selection_owner_set (NULL, 
				   selection_info->selection,
				   GDK_CURRENT_TIME, FALSE);
	  current_selections = g_list_remove_link (current_selections, 
						   tmp_list);
	  g_list_free (tmp_list);
	  g_free (selection_info);
	}
      
      tmp_list = next;
    }

  /* Remove all selection lists */
  gtk_selection_target_list_remove (widget);
}

/*************************************************************
 * gtk_selection_convert:
 *     Request the contents of a selection. When received, 
 *     a "selection_received" signal will be generated.
 *
 *   arguments:
 *     widget:	   The widget which acts as requestor
 *     selection:  Which selection to get
 *     target:	   Form of information desired (e.g., STRING)
 *     time:	   Time of request (usually of triggering event)
 *		   In emergency, you could use GDK_CURRENT_TIME
 *
 *   results:
 *     TRUE if requested succeeded. FALSE if we could not process
 *     request. (e.g., there was already a request in process for
 *     this widget). 
 *************************************************************/

gint
gtk_selection_convert (GtkWidget *widget, 
		       GdkAtom	  selection, 
		       GdkAtom	  target,
		       guint32	  time)
{
  GtkRetrievalInfo *info;
  GList *tmp_list;
  GdkWindow *owner_window;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  
  if (initialize)
    gtk_selection_init ();
  
  if (!GTK_WIDGET_REALIZED (widget))
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
  
  info = g_new (GtkRetrievalInfo, 1);
  
  info->widget = widget;
  info->selection = selection;
  info->target = target;
  info->buffer = NULL;
  info->offset = -1;
  
  /* Check if this process has current owner. If so, call handler
     procedure directly to avoid deadlocks with INCR. */
  
  owner_window = gdk_selection_owner_get (selection);
  
  if (owner_window != NULL)
    {
      GtkWidget *owner_widget;
      GtkSelectionData selection_data;
      
      selection_data.selection = selection;
      selection_data.target = target;
      selection_data.data = NULL;
      selection_data.length = -1;
      
      gdk_window_get_user_data (owner_window, (gpointer *)&owner_widget);
      
      if (owner_widget != NULL)
	{
	  gtk_selection_invoke_handler (owner_widget, 
					&selection_data,
					time);
	  
	  gtk_selection_retrieval_report (info,
					  selection_data.type, 
					  selection_data.format,
					  selection_data.data,
					  selection_data.length,
					  time);
	  
	  g_free (selection_data.data);
	  
	  g_free (info);
	  return TRUE;
	}
    }
  
  /* Otherwise, we need to go through X */
  
  current_retrievals = g_list_append (current_retrievals, info);
  gdk_selection_convert (widget->window, selection, target, time);
  gtk_timeout_add (1000, (GtkFunction) gtk_selection_retrieval_timeout, info);
  
  return TRUE;
}

/*************************************************************
 * gtk_selection_data_set:
 *     Store new data into a GtkSelectionData object. Should
 *     _only_ by called from a selection handler callback.
 *     Null terminates the stored data.
 *   arguments:
 *     type:	the type of selection data
 *     format:	format (number of bits in a unit)
 *     data:	pointer to the data (will be copied)
 *     length:	length of the data
 *   results:
 *************************************************************/

void 
gtk_selection_data_set (GtkSelectionData *selection_data,
			GdkAtom		  type,
			gint		  format,
			const guchar	 *data,
			gint		  length)
{
  if (selection_data->data)
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
	selection_data->data = g_strdup("");
    }
  
  selection_data->length = length;
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
  gtk_selection_atoms[INCR] = gdk_atom_intern ("INCR", FALSE);
  gtk_selection_atoms[MULTIPLE] = gdk_atom_intern ("MULTIPLE", FALSE);
  gtk_selection_atoms[TIMESTAMP] = gdk_atom_intern ("TIMESTAMP", FALSE);
  gtk_selection_atoms[TARGETS] = gdk_atom_intern ("TARGETS", FALSE);
}

/*************************************************************
 * gtk_selection_clear:
 *     Handler for "selection_clear_event"
 *   arguments:
 *     widget:
 *     event:
 *   results:
 *************************************************************/

gint
gtk_selection_clear (GtkWidget *widget,
		     GdkEventSelection *event)
{
  /* FIXME: there can be a problem if we change the selection
     via gtk_selection_owner_set after another client claims 
     the selection, but before we get the notification event.
     Tk filters based on serial #'s, which aren't retained by
     GTK. Filtering based on time's will be inherently 
     somewhat unreliable. */
  
  GList *tmp_list;
  GtkSelectionInfo *selection_info = NULL;
  
  tmp_list = current_selections;
  while (tmp_list)
    {
      selection_info = (GtkSelectionInfo *)tmp_list->data;
      
      if ((selection_info->selection == event->selection) &&
	  (selection_info->widget == widget))
	break;
      
      tmp_list = tmp_list->next;
    }
  
  if (tmp_list)
    {
      if (selection_info->time > event->time)
	return FALSE;		/* return FALSE to indicate that
				 * the selection was out of date,
				 * and this clear should be ignored */
      else
	{
	  current_selections = g_list_remove_link (current_selections, tmp_list);
	  g_list_free (tmp_list);
	  g_free (selection_info);
	}
    }
  
  return TRUE;
}


/*************************************************************
 * gtk_selection_request:
 *     Handler for "selection_request_event" 
 *   arguments:
 *     widget:
 *     event:
 *   results:
 *************************************************************/

gint
gtk_selection_request (GtkWidget *widget,
		       GdkEventSelection *event)
{
  GtkIncrInfo *info;
  GList *tmp_list;
  guchar *mult_atoms;
  int i;
  
  if (initialize)
    gtk_selection_init ();
  
  /* Check if we own selection */
  
  tmp_list = current_selections;
  while (tmp_list)
    {
      GtkSelectionInfo *selection_info = (GtkSelectionInfo *)tmp_list->data;
      
      if ((selection_info->selection == event->selection) &&
	  (selection_info->widget == widget))
	break;
      
      tmp_list = tmp_list->next;
    }
  
  if (tmp_list == NULL)
    return FALSE;
  
  info = g_new(GtkIncrInfo, 1);
  
  info->widget = widget;
  info->selection = event->selection;
  info->num_incrs = 0;
  
  /* Create GdkWindow structure for the requestor */
  
  info->requestor = gdk_window_lookup (event->requestor);
  if (!info->requestor)
    info->requestor = gdk_window_foreign_new (event->requestor);
  
  /* Determine conversions we need to perform */
  
  if (event->target == gtk_selection_atoms[MULTIPLE])
    {
      GdkAtom  type;
      gint     format;
      gint     length;
      
      mult_atoms = NULL;
      
      gdk_error_trap_push();
      if (!gdk_property_get (info->requestor, event->property, 0, /* AnyPropertyType */
			     0, GTK_SELECTION_MAX_SIZE, FALSE,
			     &type, &format, &length, &mult_atoms))
	{
	  gdk_selection_send_notify (event->requestor, event->selection,
				     event->target, GDK_NONE, event->time);
	  g_free (mult_atoms);
	  g_free (info);
	  return TRUE;
	}
      gdk_error_trap_pop();
      
      info->num_conversions = length / (2*sizeof (GdkAtom));
      info->conversions = g_new (GtkIncrConversion, info->num_conversions);
      
      for (i=0; i<info->num_conversions; i++)
	{
	  info->conversions[i].target = ((GdkAtom *)mult_atoms)[2*i];
	  info->conversions[i].property = ((GdkAtom *)mult_atoms)[2*i+1];
	}
    }
  else				/* only a single conversion */
    {
      info->conversions = g_new (GtkIncrConversion, 1);
      info->num_conversions = 1;
      info->conversions[0].target = event->target;
      info->conversions[0].property = event->property;
      mult_atoms = (guchar *)info->conversions;
    }
  
  /* Loop through conversions and determine which of these are big
     enough to require doing them via INCR */
  for (i=0; i<info->num_conversions; i++)
    {
      GtkSelectionData data;
      glong items;
      
      data.selection = event->selection;
      data.target = info->conversions[i].target;
      data.data = NULL;
      data.length = -1;
      
#ifdef DEBUG_SELECTION
      g_message ("Selection %ld, target %ld (%s) requested by 0x%x (property = %ld)",
		 event->selection, info->conversions[i].target,
		 gdk_atom_name(info->conversions[i].target),
		 event->requestor, event->property);
#endif
      
      gtk_selection_invoke_handler (widget, &data, event->time);
      
      if (data.length < 0)
	{
	  ((GdkAtom *)mult_atoms)[2*i+1] = GDK_NONE;
	  info->conversions[i].property = GDK_NONE;
	  continue;
	}
      
      g_return_val_if_fail ((data.format >= 8) && (data.format % 8 == 0), FALSE);
      
      items = data.length / gtk_selection_bytes_per_item (data.format);
      
      if (data.length > GTK_SELECTION_MAX_SIZE)
	{
	  /* Sending via INCR */
	  
	  info->conversions[i].offset = 0;
	  info->conversions[i].data = data;
	  info->num_incrs++;
	  
	  gdk_property_change (info->requestor, 
			       info->conversions[i].property,
			       gtk_selection_atoms[INCR],
			       32,
			       GDK_PROP_MODE_REPLACE,
			       (guchar *)&items, 1);
	}
      else
	{
	  info->conversions[i].offset = -1;
	  
	  gdk_property_change (info->requestor, 
			       info->conversions[i].property,
			       data.type,
			       data.format,
			       GDK_PROP_MODE_REPLACE,
			       data.data, items);
	  
	  g_free (data.data);
	}
    }
  
  /* If we have some INCR's, we need to send the rest of the data in
     a callback */
  
  if (info->num_incrs > 0)
    {
      /* FIXME: this could be dangerous if window doesn't still
	 exist */
      
#ifdef DEBUG_SELECTION
      g_message ("Starting INCR...");
#endif
      
      gdk_window_set_events (info->requestor,
			     gdk_window_get_events (info->requestor) |
			     GDK_PROPERTY_CHANGE_MASK);
      current_incrs = g_list_append (current_incrs, info);
      gtk_timeout_add (1000, (GtkFunction)gtk_selection_incr_timeout, info);
    }
  
  /* If it was a MULTIPLE request, set the property to indicate which
     conversions succeeded */
  if (event->target == gtk_selection_atoms[MULTIPLE])
    {
      gdk_property_change (info->requestor, event->property,
			   GDK_SELECTION_TYPE_ATOM, 32, 
			   GDK_PROP_MODE_REPLACE,
			   mult_atoms, 2*info->num_conversions);
      g_free (mult_atoms);
    }

  if (info->num_conversions == 1 &&
      info->conversions[0].property == GDK_NONE)
    {
      /* Reject the entire conversion */
      gdk_selection_send_notify (event->requestor, event->selection, 
				 event->target, GDK_NONE, event->time);
    }
  else
    {
      gdk_selection_send_notify (event->requestor, event->selection, 
				 event->target, event->property, event->time);
    }
  
  if (info->num_incrs == 0)
    {
      g_free (info->conversions);
      g_free (info);
    }
  
  return TRUE;
}

/*************************************************************
 * gtk_selection_incr_event:
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

gint
gtk_selection_incr_event (GdkWindow	   *window,
			  GdkEventProperty *event)
{
  GList *tmp_list;
  GtkIncrInfo *info = NULL;
  gint num_bytes;
  guchar *buffer;
  
  int i;
  
  if (event->state != GDK_PROPERTY_DELETE)
    return FALSE;
  
#ifdef DEBUG_SELECTION
  g_message ("PropertyDelete, property %ld", event->atom);
#endif
  
  /* Now find the appropriate ongoing INCR */
  tmp_list = current_incrs;
  while (tmp_list)
    {
      info = (GtkIncrInfo *)tmp_list->data;
      if (info->requestor == event->window)
	break;
      
      tmp_list = tmp_list->next;
    }
  
  if (tmp_list == NULL)
    return FALSE;
  
  /* Find out which target this is for */
  for (i=0; i<info->num_conversions; i++)
    {
      if (info->conversions[i].property == event->atom &&
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
	      
	      if (num_bytes > GTK_SELECTION_MAX_SIZE)
		{
		  num_bytes = GTK_SELECTION_MAX_SIZE;
		  info->conversions[i].offset += GTK_SELECTION_MAX_SIZE;
		}
	      else
		info->conversions[i].offset = -2;
	    }
#ifdef DEBUG_SELECTION
	  g_message ("INCR: put %d bytes (offset = %d) into window 0x%lx , property %ld",
		     num_bytes, info->conversions[i].offset, 
		     GDK_WINDOW_XWINDOW(info->requestor), event->atom);
#endif

	  bytes_per_item = gtk_selection_bytes_per_item (info->conversions[i].data.format);
	  gdk_property_change (info->requestor, event->atom,
			       info->conversions[i].data.type,
			       info->conversions[i].data.format,
			       GDK_PROP_MODE_REPLACE,
			       buffer,
			       num_bytes / bytes_per_item);
	  
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
      break;
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
 * gtk_selection_incr_timeout:
 *     Timeout callback for the sending portion of the INCR
 *     protocol
 *   arguments:
 *     info:	Information about this incr
 *   results:
 *************************************************************/

static gint
gtk_selection_incr_timeout (GtkIncrInfo *info)
{
  GList *tmp_list;
  gboolean retval;

  GDK_THREADS_ENTER ();
  
  /* Determine if retrieval has finished by checking if it still in
     list of pending retrievals */
  
  tmp_list = current_incrs;
  while (tmp_list)
    {
      if (info == (GtkIncrInfo *)tmp_list->data)
	break;
      tmp_list = tmp_list->next;
    }
  
  /* If retrieval is finished */
  if (!tmp_list || info->idle_time >= 5)
    {
      if (tmp_list && info->idle_time >= 5)
	{
	  current_incrs = g_list_remove_link (current_incrs, tmp_list);
	  g_list_free (tmp_list);
	}
      
      g_free (info->conversions);
      /* FIXME: we should check if requestor window is still in use,
	 and if not, remove it? */
      
      g_free (info);
      
      retval =  FALSE;		/* remove timeout */
    }
  else
    {
      info->idle_time++;
      
      retval = TRUE;		/* timeout will happen again */
    }
  
  GDK_THREADS_LEAVE ();

  return retval;
}

/*************************************************************
 * gtk_selection_notify:
 *     Handler for "selection_notify_event" signals on windows
 *     where a retrieval is currently in process. The selection
 *     owner has responded to our conversion request.
 *   arguments:
 *     widget:		Widget getting signal
 *     event:		Selection event structure
 *     info:		Information about this retrieval
 *   results:
 *     was event handled?
 *************************************************************/

gint
gtk_selection_notify (GtkWidget	       *widget,
		      GdkEventSelection *event)
{
  GList *tmp_list;
  GtkRetrievalInfo *info = NULL;
  guchar  *buffer = NULL;
  gint length;
  GdkAtom type;
  gint	  format;
  
#ifdef DEBUG_SELECTION
  g_message ("Initial receipt of selection %ld, target %ld (property = %ld)",
	     event->selection, event->target, event->property);
#endif
  
  tmp_list = current_retrievals;
  while (tmp_list)
    {
      info = (GtkRetrievalInfo *)tmp_list->data;
      if (info->widget == widget && info->selection == event->selection)
	break;
      tmp_list = tmp_list->next;
    }
  
  if (!tmp_list)		/* no retrieval in progress */
    return FALSE;

  if (event->property != GDK_NONE)
    length = gdk_selection_property_get (widget->window, &buffer, 
					 &type, &format);

  if (event->property == GDK_NONE || buffer == NULL)
    {
      current_retrievals = g_list_remove_link (current_retrievals, tmp_list);
      g_list_free (tmp_list);
      /* structure will be freed in timeout */
      gtk_selection_retrieval_report (info,
				      GDK_NONE, 0, NULL, -1, event->time);
      
      return TRUE;
    }
  
  if (type == gtk_selection_atoms[INCR])
    {
      /* The remainder of the selection will come through PropertyNotify
	 events */

      info->notify_time = event->time;
      info->idle_time = 0;
      info->offset = 0;		/* Mark as OK to proceed */
      gdk_window_set_events (widget->window,
			     gdk_window_get_events (widget->window)
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
				      buffer, length, event->time);
    }
  
  gdk_property_delete (widget->window, event->property);
  
  g_free (buffer);
  
  return TRUE;
}

/*************************************************************
 * gtk_selection_property_notify:
 *     Handler for "property_notify_event" signals on windows
 *     where a retrieval is currently in process. The selection
 *     owner has added more data.
 *   arguments:
 *     widget:		Widget getting signal
 *     event:		Property event structure
 *     info:		Information about this retrieval
 *   results:
 *     was event handled?
 *************************************************************/

gint
gtk_selection_property_notify (GtkWidget	*widget,
			       GdkEventProperty *event)
{
  GList *tmp_list;
  GtkRetrievalInfo *info = NULL;
  guchar *new_buffer;
  int length;
  GdkAtom type;
  gint	  format;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if ((event->state != GDK_PROPERTY_NEW_VALUE) ||  /* property was deleted */
      (event->atom != gdk_selection_property)) /* not the right property */
    return FALSE;
  
#ifdef DEBUG_SELECTION
  g_message ("PropertyNewValue, property %ld",
	     event->atom);
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
  
  length = gdk_selection_property_get (widget->window, &new_buffer, 
				       &type, &format);
  gdk_property_delete (widget->window, event->atom);
  
  /* We could do a lot better efficiency-wise by paying attention to
     what length was sent in the initial INCR transaction, instead of
     doing memory allocation at every step. But its only guaranteed to
     be a _lower bound_ (pretty useless!) */
  
  if (length == 0 || type == GDK_NONE)		/* final zero length portion */
    {
      /* Info structure will be freed in timeout */
      current_retrievals = g_list_remove_link (current_retrievals, tmp_list);
      g_list_free (tmp_list);
      gtk_selection_retrieval_report (info,
				      type, format, 
				      (type == GDK_NONE) ?  NULL : info->buffer,
				      (type == GDK_NONE) ?  -1 : info->offset,
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

static gint
gtk_selection_retrieval_timeout (GtkRetrievalInfo *info)
{
  GList *tmp_list;
  gboolean retval;

  GDK_THREADS_ENTER ();
  
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
  if (!tmp_list || info->idle_time >= 5)
    {
      if (tmp_list && info->idle_time >= 5)
	{
	  current_retrievals = g_list_remove_link (current_retrievals, tmp_list);
	  g_list_free (tmp_list);
	  gtk_selection_retrieval_report (info, GDK_NONE, 0, NULL, -1, GDK_CURRENT_TIME);
	}
      
      g_free (info->buffer);
      g_free (info);
      
      retval =  FALSE;		/* remove timeout */
    }
  else
    {
      info->idle_time++;
      
      retval =  TRUE;		/* timeout will happen again */
    }

  GDK_THREADS_LEAVE ();

  return retval;
}

/*************************************************************
 * gtk_selection_retrieval_report:
 *     Emits a "selection_received" signal.
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
  
  gtk_signal_emit_by_name (GTK_OBJECT(info->widget),
			   "selection_received", 
			   &data, time);
}

/*************************************************************
 * gtk_selection_invoke_handler:
 *     Finds and invokes handler for specified
 *     widget/selection/target combination, calls 
 *     gtk_selection_default_handler if none exists.
 *
 *   arguments:
 *     widget:	    selection owner
 *     data:	    selection data [INOUT]
 *     time:        time from requeset
 *     
 *   results:
 *     Number of bytes written to buffer, -1 if error
 *************************************************************/

static void
gtk_selection_invoke_handler (GtkWidget	       *widget,
			      GtkSelectionData *data,
			      guint             time)
{
  GtkTargetList *target_list;
  guint info;
  

  g_return_if_fail (widget != NULL);

  target_list = gtk_selection_target_list_get (widget, data->selection);
  if (target_list && 
      gtk_target_list_find (target_list, data->target, &info))
    {
      gtk_signal_emit_by_name (GTK_OBJECT (widget), 
			       "selection_get",
			       data,
			       info, time);
    }
  else
    gtk_selection_default_handler (widget, data);
}

/*************************************************************
 * gtk_selection_default_handler:
 *     Handles some default targets that exist for any widget
 *     If it can't fit results into buffer, returns -1. This
 *     won't happen in any conceivable case, since it would
 *     require 1000 selection targets!
 *
 *   arguments:
 *     widget:	    selection owner
 *     data:	    selection data [INOUT]
 *
 *************************************************************/

static void
gtk_selection_default_handler (GtkWidget	*widget,
			       GtkSelectionData *data)
{
  if (data->target == gtk_selection_atoms[TIMESTAMP])
    {
      /* Time which was used to obtain selection */
      GList *tmp_list;
      GtkSelectionInfo *selection_info;
      
      tmp_list = current_selections;
      while (tmp_list)
	{
	  selection_info = (GtkSelectionInfo *)tmp_list->data;
	  if ((selection_info->widget == widget) &&
	      (selection_info->selection == data->selection))
	    {
	      gulong time = selection_info->time;

	      gtk_selection_data_set (data,
				      GDK_SELECTION_TYPE_INTEGER,
				      32,
				      (guchar *)&time,
				      sizeof (time));
	      return;
	    }
	  
	  tmp_list = tmp_list->next;
	}
      
      data->length = -1;
    }
  else if (data->target == gtk_selection_atoms[TARGETS])
    {
      /* List of all targets supported for this widget/selection pair */
      GdkAtom *p;
      guint count;
      GList *tmp_list;
      GtkTargetList *target_list;
      GtkTargetPair *pair;
      
      target_list = gtk_selection_target_list_get (widget,
						   data->selection);
      count = g_list_length (target_list->list) + 3;
      
      data->type = GDK_SELECTION_TYPE_ATOM;
      data->format = 32;
      data->length = count * sizeof (GdkAtom);
      
      p = g_new (GdkAtom, count);
      data->data = (guchar *)p;
      
      *p++ = gtk_selection_atoms[TIMESTAMP];
      *p++ = gtk_selection_atoms[TARGETS];
      *p++ = gtk_selection_atoms[MULTIPLE];
      
      tmp_list = target_list->list;
      while (tmp_list)
	{
	  pair = (GtkTargetPair *)tmp_list->data;
	  *p++ = pair->target;
	  
	  tmp_list = tmp_list->next;
	}
    }
  else
    {
      data->length = -1;
    }
}


GtkSelectioData*
gtk_selection_data_copy (GtkSelectionData *data)
{
  GtkSelectionData *new_data;
  
  g_return_val_if_fail (data != NULL, NULL);
  
  new_data = g_new (GtkSelectionData, 1);
  *new_data = *data;
  
  return new_data;
}

void
gtk_selection_data_free (GtkSelectionData *data)
{
  g_return_if_fail (data != NULL);
  
  g_free (data);
}

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
