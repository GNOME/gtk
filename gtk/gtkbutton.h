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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_BUTTON_H__
#define __GTK_BUTTON_H__


#include <gtk/gtkbin.h>
#include <gtk/gtkimage.h>


G_BEGIN_DECLS

#define GTK_TYPE_BUTTON                 (gtk_button_get_type ())
#define GTK_BUTTON(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_BUTTON, GtkButton))
#define GTK_BUTTON_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_BUTTON, GtkButtonClass))
#define GTK_IS_BUTTON(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_BUTTON))
#define GTK_IS_BUTTON_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_BUTTON))
#define GTK_BUTTON_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_BUTTON, GtkButtonClass))

typedef struct _GtkButton             GtkButton;
typedef struct _GtkButtonPrivate      GtkButtonPrivate;
typedef struct _GtkButtonClass        GtkButtonClass;

struct _GtkButton
{
  /*< private >*/
  GtkBin bin;

  GtkButtonPrivate *priv;
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
  
  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};


GType          gtk_button_get_type          (void) G_GNUC_CONST;
GtkWidget*     gtk_button_new               (void);
GtkWidget*     gtk_button_new_with_label    (const gchar    *label);
GtkWidget*     gtk_button_new_from_stock    (const gchar    *stock_id);
GtkWidget*     gtk_button_new_with_mnemonic (const gchar    *label);
void           gtk_button_clicked           (GtkButton      *button);
GDK_DEPRECATED
void           gtk_button_pressed           (GtkButton      *button);
GDK_DEPRECATED
void           gtk_button_released          (GtkButton      *button);
GDK_DEPRECATED
void           gtk_button_enter             (GtkButton      *button);
GDK_DEPRECATED
void           gtk_button_leave             (GtkButton      *button);

void                  gtk_button_set_relief         (GtkButton      *button,
						     GtkReliefStyle  newstyle);
GtkReliefStyle        gtk_button_get_relief         (GtkButton      *button);
void                  gtk_button_set_label          (GtkButton      *button,
						     const gchar    *label);
const gchar *         gtk_button_get_label          (GtkButton      *button);
void                  gtk_button_set_use_underline  (GtkButton      *button,
						     gboolean        use_underline);
gboolean              gtk_button_get_use_underline  (GtkButton      *button);
void                  gtk_button_set_use_stock      (GtkButton      *button,
						     gboolean        use_stock);
gboolean              gtk_button_get_use_stock      (GtkButton      *button);
void                  gtk_button_set_focus_on_click (GtkButton      *button,
						     gboolean        focus_on_click);
gboolean              gtk_button_get_focus_on_click (GtkButton      *button);
void                  gtk_button_set_alignment      (GtkButton      *button,
						     gfloat          xalign,
						     gfloat          yalign);
void                  gtk_button_get_alignment      (GtkButton      *button,
						     gfloat         *xalign,
						     gfloat         *yalign);
void                  gtk_button_set_image          (GtkButton      *button,
					             GtkWidget      *image);
GtkWidget*            gtk_button_get_image          (GtkButton      *button);
void                  gtk_button_set_image_position (GtkButton      *button,
						     GtkPositionType position);
GtkPositionType       gtk_button_get_image_position (GtkButton      *button);
GDK_AVAILABLE_IN_3_6
void                  gtk_button_set_always_show_image (GtkButton   *button,
                                                        gboolean     always_show);
GDK_AVAILABLE_IN_3_6
gboolean              gtk_button_get_always_show_image (GtkButton   *button);

GdkWindow*            gtk_button_get_event_window   (GtkButton      *button);


G_END_DECLS

#endif /* __GTK_BUTTON_H__ */
