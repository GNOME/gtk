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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_COLORSEL_H__
#define __GTK_COLORSEL_H__

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <gdk/gdk.h>

#include "gtkwindow.h"
#include "gtkvbox.h"
#include "gtkframe.h"
#include "gtkpreview.h"
#include "gtkbutton.h"
#include "gtkentry.h"
#include "gtkhbox.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtkmisc.h"
#include "gtkrange.h"
#include "gtkscale.h"
#include "gtkhscale.h"
#include "gtktable.h"
#include "gtkeventbox.h"


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
void       gtk_color_selection_class_init        (GtkColorSelectionClass *klass);
void       gtk_color_selection_init              (GtkColorSelection      *colorsel);

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
void       gtk_color_selection_dialog_class_init (GtkColorSelectionDialogClass *klass);
void       gtk_color_selection_dialog_init       (GtkColorSelectionDialog      *colorseldiag);

GtkWidget* gtk_color_selection_dialog_new        (const gchar *title);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_COLORSEL_H__ */
