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
#ifndef __GTK_MENU_ITEM_H__
#define __GTK_MENU_ITEM_H__


#include <gdk/gdk.h>
#include <gtk/gtkitem.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_MENU_ITEM(obj)          GTK_CHECK_CAST (obj, gtk_menu_item_get_type (), GtkMenuItem)
#define GTK_MENU_ITEM_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_menu_item_get_type (), GtkMenuItemClass)
#define GTK_IS_MENU_ITEM(obj)       GTK_CHECK_TYPE (obj, gtk_menu_item_get_type ())


typedef struct _GtkMenuItem       GtkMenuItem;
typedef struct _GtkMenuItemClass  GtkMenuItemClass;

struct _GtkMenuItem
{
  GtkItem item;

  GtkWidget *submenu;

  guint   accelerator_signal;
  gchar   accelerator_key;
  guint8  accelerator_mods;
  guint16 accelerator_size;
  guint16 toggle_size;

  guint show_toggle_indicator : 1;
  guint show_submenu_indicator : 1;
  guint submenu_placement : 1;
  guint submenu_direction : 1;
  guint right_justify: 1;
  gint timer;
};

struct _GtkMenuItemClass
{
  GtkItemClass parent_class;

  gint toggle_size;

  gchar *shift_text;
  gchar *control_text;
  gchar *alt_text;
  gchar *separator_text;

  void (* activate) (GtkMenuItem *menu_item);
};


guint      gtk_menu_item_get_type         (void);
GtkWidget* gtk_menu_item_new              (void);
GtkWidget* gtk_menu_item_new_with_label   (const gchar         *label);
void       gtk_menu_item_set_submenu      (GtkMenuItem         *menu_item,
					   GtkWidget           *submenu);
void       gtk_menu_item_remove_submenu   (GtkMenuItem         *menu_item);
void       gtk_menu_item_set_placement    (GtkMenuItem         *menu_item,
					   GtkSubmenuPlacement  placement);
void       gtk_menu_item_accelerator_size (GtkMenuItem         *menu_item);
void       gtk_menu_item_accelerator_text (GtkMenuItem         *menu_item,
					   gchar               *buffer);
void       gtk_menu_item_configure        (GtkMenuItem         *menu_item,
					   gint                 show_toggle_indicator,
					   gint                 show_submenu_indicator);
void       gtk_menu_item_select           (GtkMenuItem         *menu_item);
void       gtk_menu_item_deselect         (GtkMenuItem         *menu_item);
void       gtk_menu_item_activate         (GtkMenuItem         *menu_item);
void       gtk_menu_item_right_justify    (GtkMenuItem         *menu_item);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_MENU_ITEM_H__ */
