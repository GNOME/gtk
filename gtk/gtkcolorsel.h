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
#ifndef __GTK_COLORSEL_H__
#define __GTK_COLORSEL_H__

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <gdk/gdk.h>

#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkpreview.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmisc.h>
#include <gtk/gtkrange.h>
#include <gtk/gtkscale.h>
#include <gtk/gtkhscale.h>
#include <gtk/gtktable.h>
#include <gtk/gtkeventbox.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_COLOR_SELECTION(obj)          GTK_CHECK_CAST (obj, gtk_color_selection_get_type (), GtkColorSelection)
#define GTK_COLOR_SELECTION_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_color_selection_get_type (), GtkColorSelectionClass)
#define GTK_IS_COLOR_SELECTION(obj)       GTK_CHECK_TYPE (obj, gtk_color_selection_get_type ())

#define GTK_COLOR_SELECTION_DIALOG(obj)          GTK_CHECK_CAST (obj, gtk_color_selection_dialog_get_type (), GtkColorSelectionDialog)
#define GTK_COLOR_SELECTION_DIALOG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_color_selection_dialog_get_type (), GtkColorSelectionDialogClass)
#define GTK_IS_COLOR_SELECTION_DIALOG(obj)       GTK_CHECK_TYPE (obj, gtk_color_selection_dialog_get_type ())


typedef struct _GtkColorSelection             GtkColorSelection;
typedef struct _GtkColorSelectionClass        GtkColorSelectionClass;

typedef struct _GtkColorSelectionDialog       GtkColorSelectionDialog;
typedef struct _GtkColorSelectionDialogClass  GtkColorSelectionDialogClass;


struct _GtkColorSelection
{
  GtkVBox vbox;

  GtkWidget *wheel_area;
  GtkWidget *value_area;
  GtkWidget *sample_area;
  GtkWidget *sample_area_eb;

  GtkWidget *scales[8];
  GtkWidget *entries[8];
  GtkWidget *opacity_label;

  GdkGC *wheel_gc;
  GdkGC *value_gc;
  GdkGC *sample_gc;

  GtkUpdateType policy;
  gint use_opacity;
  gint timer_active;
  gint timer_tag;
  gdouble values[8];
  gdouble old_values[8];

  guchar *wheel_buf;
  guchar *value_buf;
  guchar *sample_buf;
};

struct _GtkColorSelectionClass
{
  GtkVBoxClass parent_class;

  void (* color_changed) (GtkColorSelection *colorsel);
};

struct _GtkColorSelectionDialog
{
  GtkWindow window;

  GtkWidget *colorsel;
  GtkWidget *main_vbox;
  GtkWidget *ok_button;
  GtkWidget *reset_button;
  GtkWidget *cancel_button;
  GtkWidget *help_button;
};

struct _GtkColorSelectionDialogClass
{
  GtkWindowClass parent_class;
};


/* ColorSelection */

guint      gtk_color_selection_get_type          (void);

GtkWidget* gtk_color_selection_new               (void);

void       gtk_color_selection_set_update_policy (GtkColorSelection     *colorsel,
                                                  GtkUpdateType          policy);

void       gtk_color_selection_set_opacity       (GtkColorSelection     *colorsel,
                                                  gint                   use_opacity);

void       gtk_color_selection_set_color         (GtkColorSelection     *colorsel,
					          gdouble               *color);

void       gtk_color_selection_get_color         (GtkColorSelection     *colorsel,
                                                  gdouble               *color);

/* ColorSelectionDialog */

guint      gtk_color_selection_dialog_get_type   (void);

GtkWidget* gtk_color_selection_dialog_new        (const gchar *title);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_COLORSEL_H__ */
