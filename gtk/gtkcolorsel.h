/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc. 
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
#ifndef __GTK_COLOR_SELECTION_H__
#define __GTK_COLOR_SELECTION_H__

#include <gtk/gtkdialog.h>
#include <gtk/gtkvbox.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_COLOR_SELECTION			(gtk_color_selection_get_type ())
#define GTK_COLOR_SELECTION(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_COLOR_SELECTION, GtkColorSelection))
#define GTK_COLOR_SELECTION_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_COLOR_SELECTION, GtkColorSelectionClass))
#define GTK_IS_COLOR_SELECTION(obj)			(GTK_CHECK_TYPE ((obj), GTK_TYPE_COLOR_SELECTION))
#define GTK_IS_COLOR_SELECTION_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_COLOR_SELECTION))
#define GTK_COLOR_SELECTION_GET_CLASS(obj)              (GTK_CHECK_GET_CLAS ((obj), GTK_TYPE_COLOR_SELECTION, GtkColorSelectionClass))


/* Number of elements in the custom palatte */
#define GTK_CUSTOM_PALETTE_WIDTH 10
#define GTK_CUSTOM_PALETTE_HEIGHT 2

typedef struct _GtkColorSelection       GtkColorSelection;
typedef struct _GtkColorSelectionClass  GtkColorSelectionClass;


struct _GtkColorSelection
{
  GtkVBox parent_instance;

  /* < private_data > */
  gpointer private_data;
};

struct _GtkColorSelectionClass
{
  GtkVBoxClass parent_class;

  void (*color_changed)	(GtkColorSelection *color_selection);
};


/* ColorSelection */ 

GtkType     gtk_color_selection_get_type          (void);
GtkWidget * gtk_color_selection_new               (void);
void        gtk_color_selection_set_update_policy (GtkColorSelection *colorsel,
						   GtkUpdateType      policy);
gboolean    gtk_color_selection_get_use_opacity   (GtkColorSelection *colorsel);
void        gtk_color_selection_set_use_opacity   (GtkColorSelection *colorsel,
						   gboolean           use_opacity);
gboolean    gtk_color_selection_get_use_palette   (GtkColorSelection *colorsel);
void        gtk_color_selection_set_use_palette   (GtkColorSelection *colorsel,
						   gboolean           use_palette);

/* The Color set is an array of doubles, of the following format:
 * color[0] = red_channel;
 * color[1] = green_channel;
 * color[2] = blue_channel;
 * color[3] = alpha_channel;
 */
void       gtk_color_selection_set_color           (GtkColorSelection    *colorsel,
						    gdouble               *color);
void       gtk_color_selection_get_color           (GtkColorSelection    *colorsel,
						    gdouble               *color);
void       gtk_color_selection_set_old_color       (GtkColorSelection    *colorsel,
						    gdouble               *color);
void       gtk_color_selection_get_old_color       (GtkColorSelection    *colorsel,
						    gdouble               *color);
void       gtk_color_selection_set_palette_color   (GtkColorSelection   *colorsel,
						    gint                  x,
						    gint                  y,
						    gdouble              *color);
gboolean   gtk_color_selection_get_palette_color   (GtkColorSelection   *colorsel,
						    gint                  x,
						    gint                  y,
						    gdouble              *color);
void       gtk_color_selection_unset_palette_color (GtkColorSelection   *colorsel,
						    gint                  x,
						    gint                  y);
gboolean   gtk_color_selection_is_adjusting        (GtkColorSelection    *colorsel);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_COLOR_SELECTION_H__ */
