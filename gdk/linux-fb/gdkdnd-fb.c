/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include <string.h>

#include "gdk.h"          /* For gdk_flush() */
#include "gdkdnd.h"
#include "gdkproperty.h"
#include "gdkinternals.h"
#include "gdkprivate-fb.h"

typedef struct _GdkDragContextPrivate GdkDragContextPrivate;

typedef enum {
  GDK_DRAG_STATUS_DRAG,
  GDK_DRAG_STATUS_MOTION_WAIT,
  GDK_DRAG_STATUS_ACTION_WAIT,
  GDK_DRAG_STATUS_DROP
} GtkDragStatus;

/* Structure that holds information about a drag in progress.
 * this is used on both source and destination sides.
 */
struct _GdkDragContextPrivate {
  GdkDragContext context;

  guint   ref_count;
};

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
  g_return_if_fail (context != NULL);

  ((GdkDragContextPrivate *)context)->ref_count++;
}

void            
gdk_drag_context_unref (GdkDragContext *context)
{
  GdkDragContextPrivate *private = (GdkDragContextPrivate *)context;

  g_return_if_fail (context != NULL);
  g_return_if_fail (private->ref_count > 0);

  private->ref_count--;
  
  if (private->ref_count == 0)
    {
      g_dataset_destroy (private);
      
      g_list_free (context->targets);

      if (context->source_window)
	{
#if 0
	  if ((context->protocol == GDK_DRAG_PROTO_XDND) &&
	      !context->is_source)
	    xdnd_manage_source_filter (context, context->source_window, FALSE);
#endif

	  gdk_window_unref (context->source_window);
	}

      if (context->dest_window)
	gdk_window_unref (context->dest_window);

#if 0
      if (private->window_cache)
	gdk_window_cache_destroy (private->window_cache);
#endif

      contexts = g_list_remove (contexts, private);
      g_free (private);
    }
}

/*************************************************************
 ************************** Public API ***********************
 *************************************************************/

void
gdk_dnd_init (void)
{
}		      

/* Source side */

static void
gdk_drag_do_leave (GdkDragContext *context, guint32 time) G_GNUC_UNUSED;

static void
gdk_drag_do_leave (GdkDragContext *context, guint32 time)
{
}

GdkDragContext * 
gdk_drag_begin (GdkWindow     *window,
		GList         *targets)
{
  GList *tmp_list;
  GdkDragContext *new_context;
  
  g_return_val_if_fail (window != NULL, NULL);

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
  *protocol = GDK_DRAG_PROTO_NONE;
  return GDK_NONE;
}

void
gdk_drag_find_window (GdkDragContext  *context,
		      GdkWindow       *drag_window,
		      gint             x_root,
		      gint             y_root,
		      GdkWindow      **dest_window,
		      GdkDragProtocol *protocol)
{
  g_return_if_fail (context != NULL);

  *dest_window = gdk_window_get_pointer(NULL, &x_root, &y_root, NULL);
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
  g_return_val_if_fail (context != NULL, FALSE);

  return FALSE;
}

void
gdk_drag_drop (GdkDragContext *context,
	       guint32         time)
{
  g_return_if_fail (context != NULL);
}

void
gdk_drag_abort (GdkDragContext *context,
		guint32         time)
{
  g_return_if_fail (context != NULL);
}

/* Destination side */

void             
gdk_drag_status (GdkDragContext   *context,
		 GdkDragAction     action,
		 guint32           time)
{
  g_return_if_fail (context != NULL);
}

void 
gdk_drop_reply (GdkDragContext   *context,
		gboolean          ok,
		guint32           time)
{
  g_return_if_fail (context != NULL);
}

void             
gdk_drop_finish (GdkDragContext   *context,
		 gboolean          success,
		 guint32           time)
{
  g_return_if_fail (context != NULL);
}


void            
gdk_window_register_dnd (GdkWindow      *window)
{
  g_return_if_fail (window != NULL);
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
  g_return_val_if_fail (context != NULL, GDK_NONE);

  return GDK_NONE;
}

