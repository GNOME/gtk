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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
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
typedef struct _GtkTargetList    GtkTargetList;
typedef struct _GtkTargetEntry   GtkTargetEntry;

struct _GtkTargetEntry {
  gchar *target;
  guint  flags;
  guint  info;
};

/* These structures not public, and are here only for the convenience of
 * gtkdnd.c 
 */

typedef struct _GtkTargetPair GtkTargetPair;

/* This structure is a list of destinations, and associated guint id's */
struct _GtkTargetList {
  GList *list;
  guint ref_count;
};

struct _GtkTargetPair {
  GdkAtom   target;
  guint     flags;
  guint     info;
};

GtkTargetList *gtk_target_list_new       (const GtkTargetEntry *targets,
					  guint                 ntargets);
void           gtk_target_list_ref       (GtkTargetList  *list);
void           gtk_target_list_unref     (GtkTargetList  *list);
void           gtk_target_list_add       (GtkTargetList  *list,
				  	  GdkAtom         target,
					  guint           flags,
					  guint           info);
void           gtk_target_list_add_table (GtkTargetList        *list,
					  const GtkTargetEntry *targets,
					  guint                 ntargets);
void           gtk_target_list_remove    (GtkTargetList  *list,
					  GdkAtom         target);
gboolean       gtk_target_list_find      (GtkTargetList  *list,
					  GdkAtom         target,
					  guint          *info);

/* Public interface */

gint gtk_selection_owner_set (GtkWidget 	  *widget,
			      GdkAtom    	   selection,
			      guint32    	   time);
void gtk_selection_add_target (GtkWidget           *widget, 
			       GdkAtom              selection,
			       GdkAtom              target,
			       guint                info);
void gtk_selection_add_targets (GtkWidget            *widget, 
				GdkAtom               selection,
				const GtkTargetEntry *targets,
				guint                 ntargets);
gint gtk_selection_convert   (GtkWidget 	  *widget, 
			      GdkAtom    	   selection, 
			      GdkAtom    	   target,
			      guint32    	   time);


void gtk_selection_data_set (GtkSelectionData *selection_data,
			     GdkAtom           type,
			     gint              format,
			     const guchar     *data,
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
GtkSelectioData *gtk_selection_data_copy (GtkSelectionData *data);
void		 gtk_selection_data_free (GtkSelectionData *data);



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SELECTION_H__ */
