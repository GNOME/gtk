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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_BUTTON_H__
#define __GTK_BUTTON_H__


#include <gdk/gdk.h>
#include <gtk/gtkbin.h>
#include <gtk/gtkenums.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_BUTTON                 (gtk_button_get_type ())
#define GTK_BUTTON(obj)                 (GTK_CHECK_CAST ((obj), GTK_TYPE_BUTTON, GtkButton))
#define GTK_BUTTON_CLASS(klass)         (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_BUTTON, GtkButtonClass))
#define GTK_IS_BUTTON(obj)              (GTK_CHECK_TYPE ((obj), GTK_TYPE_BUTTON))
#define GTK_IS_BUTTON_CLASS(klass)      (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_BUTTON))
#define GTK_BUTTON_GET_CLASS(obj)       (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_BUTTON, GtkButtonClass))


typedef struct _GtkButton       GtkButton;
typedef struct _GtkButtonClass  GtkButtonClass;

struct _GtkButton
{
  GtkBin bin;

  gchar *label_text;

  guint activate_timeout;

  guint constructed : 1;
  guint in_button : 1;
  guint button_down : 1;
  guint relief : 2;
  guint use_underline : 1;
  guint use_stock : 1;
  guint depressed : 1;
};

struct _GtkButtonClass
{
  GtkBinClass        parent_class;
  
  void (* pressed)  (GtkButton *button);
  void (* released) (GtkButton *button);
  void (* clicked)  (GtkButton *button);
  void (* enter)    (GtkButton *button);
  void (* leave)    (GtkButton *button);
  void (* activate) (GtkButton *button);
};


GtkType        gtk_button_get_type          (void) G_GNUC_CONST;
GtkWidget*     gtk_button_new               (void);
GtkWidget*     gtk_button_new_with_label    (const gchar    *label);
GtkWidget*     gtk_button_new_from_stock    (const gchar    *stock_id);
GtkWidget*     gtk_button_new_with_mnemonic (const gchar    *label);
void           gtk_button_pressed           (GtkButton      *button);
void           gtk_button_released          (GtkButton      *button);
void           gtk_button_clicked           (GtkButton      *button);
void           gtk_button_enter             (GtkButton      *button);
void           gtk_button_leave             (GtkButton      *button);
void           gtk_button_set_relief        (GtkButton      *button,
					     GtkReliefStyle  newstyle);
GtkReliefStyle gtk_button_get_relief        (GtkButton      *button);

void                  gtk_button_set_label         (GtkButton   *button,
						    const gchar *label);
G_CONST_RETURN gchar *gtk_button_get_label         (GtkButton   *button);
void                  gtk_button_set_use_underline (GtkButton   *button,
						    gboolean     use_underline);
gboolean              gtk_button_get_use_underline (GtkButton   *button);
void                  gtk_button_set_use_stock     (GtkButton   *button,
						    gboolean     use_stock);
gboolean              gtk_button_get_use_stock     (GtkButton   *button);
void                  _gtk_button_set_depressed    (GtkButton   *button,
						    gboolean     depressed);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_BUTTON_H__ */
