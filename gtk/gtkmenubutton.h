/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2003 Ricardo Fernandez Pascual
 * Copyright (C) 2004 Paolo Borelli
 * Copyright (C) 2012 Bastien Nocera
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

#ifndef __GTK_MENU_BUTTON_H__
#define __GTK_MENU_BUTTON_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktogglebutton.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkpopover.h>

G_BEGIN_DECLS

#define GTK_TYPE_MENU_BUTTON            (gtk_menu_button_get_type ())
#define GTK_MENU_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MENU_BUTTON, GtkMenuButton))
#define GTK_IS_MENU_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_MENU_BUTTON))

typedef struct _GtkMenuButton        GtkMenuButton;

/**
 * GtkMenuButtonCreatePopupFunc:
 * @menu_button: the #GtkMenuButton
 *
 * User-provided callback function to create a popup for @menu_button on demand.
 * This function is called when the popoup of @menu_button is shown, but none has
 * been provided via gtk_menu_buton_set_popup(), gtk_menu_button_set_popover()
 * or gtk_menu_button_set_menu_model().
 */
typedef void  (*GtkMenuButtonCreatePopupFunc) (GtkMenuButton *menu_button,
                                               gpointer       user_data);

GDK_AVAILABLE_IN_ALL
GType        gtk_menu_button_get_type       (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GtkWidget   *gtk_menu_button_new            (void);

GDK_AVAILABLE_IN_ALL
void         gtk_menu_button_set_popup      (GtkMenuButton *menu_button,
                                             GtkWidget     *menu);
GDK_AVAILABLE_IN_ALL
GtkMenu     *gtk_menu_button_get_popup      (GtkMenuButton *menu_button);

GDK_AVAILABLE_IN_ALL
void         gtk_menu_button_set_popover    (GtkMenuButton *menu_button,
                                             GtkWidget     *popover);
GDK_AVAILABLE_IN_ALL
GtkPopover  *gtk_menu_button_get_popover    (GtkMenuButton *menu_button);

GDK_AVAILABLE_IN_ALL
void         gtk_menu_button_set_direction  (GtkMenuButton *menu_button,
                                             GtkArrowType   direction);
GDK_AVAILABLE_IN_ALL
GtkArrowType gtk_menu_button_get_direction  (GtkMenuButton *menu_button);

GDK_AVAILABLE_IN_ALL
void         gtk_menu_button_set_menu_model (GtkMenuButton *menu_button,
                                             GMenuModel    *menu_model);
GDK_AVAILABLE_IN_ALL
GMenuModel  *gtk_menu_button_get_menu_model (GtkMenuButton *menu_button);

GDK_AVAILABLE_IN_ALL
void         gtk_menu_button_set_align_widget (GtkMenuButton *menu_button,
                                               GtkWidget     *align_widget);
GDK_AVAILABLE_IN_ALL
GtkWidget   *gtk_menu_button_get_align_widget (GtkMenuButton *menu_button);

GDK_AVAILABLE_IN_ALL
void         gtk_menu_button_set_use_popover (GtkMenuButton *menu_button,
                                              gboolean       use_popover);

GDK_AVAILABLE_IN_ALL
gboolean     gtk_menu_button_get_use_popover (GtkMenuButton *menu_button);

GDK_AVAILABLE_IN_ALL
void         gtk_menu_button_set_icon_name (GtkMenuButton *menu_button,
                                            const char    *icon_name);
GDK_AVAILABLE_IN_ALL
const char * gtk_menu_button_get_icon_name (GtkMenuButton *menu_button);

GDK_AVAILABLE_IN_ALL
void         gtk_menu_button_set_label (GtkMenuButton *menu_button,
                                        const char    *label);
GDK_AVAILABLE_IN_ALL
const char * gtk_menu_button_get_label (GtkMenuButton *menu_button);

GDK_AVAILABLE_IN_ALL
void           gtk_menu_button_set_relief   (GtkMenuButton  *menu_button,
                                             GtkReliefStyle  relief);
GDK_AVAILABLE_IN_ALL
GtkReliefStyle gtk_menu_button_get_relief   (GtkMenuButton  *menu_button);

GDK_AVAILABLE_IN_ALL
void          gtk_menu_button_popup (GtkMenuButton *menu_button);
GDK_AVAILABLE_IN_ALL
void          gtk_menu_button_popdown (GtkMenuButton *menu_button);

GDK_AVAILABLE_IN_ALL
void          gtk_menu_button_set_create_popup_func (GtkMenuButton                *menu_button,
                                                     GtkMenuButtonCreatePopupFunc  func,
                                                     gpointer                      user_data,
                                                     GDestroyNotify                destroy_notify);

G_END_DECLS

#endif /* __GTK_MENU_BUTTON_H__ */
