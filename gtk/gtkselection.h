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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GTK_SELECTION_H__
#define __GTK_SELECTION_H__


#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GtkSelectionData GtkSelectioData;

/* a callback function that provides the selection. Arguments are:
   widget: selection owner
   offset: offset into selection
   buffer: buffer into which to store selection 
   length: length of buffer
   bytes_after: (sizeof(selection) - offset - length ) (return)
   data:   callback data */

typedef void (*GtkSelectionFunction) (GtkWidget *widget, 
				      GtkSelectionData *selection_data,
				      gpointer data);

/* Public interface */

gint gtk_selection_owner_set (GtkWidget 	  *widget,
			      GdkAtom    	   selection,
			      guint32    	   time);
void gtk_selection_add_handler (GtkWidget           *widget, 
				GdkAtom              selection,
				GdkAtom              target,
				GtkSelectionFunction function,
				gpointer             data);
void gtk_selection_add_handler_full (GtkWidget           *widget, 
				     GdkAtom              selection,
				     GdkAtom              target,
				     GtkSelectionFunction function,
				     GtkCallbackMarshal   marshal,
				     gpointer             data,
				     GtkDestroyNotify     destroy);
gint gtk_selection_convert   (GtkWidget 	  *widget, 
			      GdkAtom    	   selection, 
			      GdkAtom    	   target,
			      guint32    	   time);


void gtk_selection_data_set (GtkSelectionData *selection_data,
			     GdkAtom           type,
			     gint              format,
			     guchar           *data,
			     gint              length);

/* Called when a widget is destroyed */

void gtk_selection_remove_all      (GtkWidget *widget);

/* Event handlers */

gint gtk_selection_clear           (GtkWidget 	      *widget,
				    GdkEventSelection *event);
gint gtk_selection_request         (GtkWidget  	      *widget,
				    GdkEventSelection *event);
gint gtk_selection_incr_event      (GdkWindow         *window,
				    GdkEventProperty  *event);
gint gtk_selection_notify          (GtkWidget         *widget,
				    GdkEventSelection *event);
gint gtk_selection_property_notify (GtkWidget         *widget,
				    GdkEventProperty  *event);



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SELECTION_H__ */
