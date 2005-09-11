/* gtkcellrendererkeys.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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

#ifndef __GTK_CELL_RENDERER_KEYS_H__
#define __GTK_CELL_RENDERER_KEYS_H__

#include "gtkcellrenderertext.h"

G_BEGIN_DECLS

#define GTK_TYPE_CELL_RENDERER_KEYS		(gtk_cell_renderer_keys_get_type ())
#define GTK_CELL_RENDERER_KEYS(obj)		(GTK_CHECK_CAST ((obj), GTK_TYPE_CELL_RENDERER_KEYS, GtkCellRendererKeys))
#define GTK_CELL_RENDERER_KEYS_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_RENDERER_KEYS, GtkCellRendererKeysClass))
#define GTK_IS_CELL_RENDERER_KEYS(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_CELL_RENDERER_KEYS))
#define GTK_IS_CELL_RENDERER_KEYS_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_RENDERER_KEYS))
#define GTK_CELL_RENDERER_KEYS_GET_CLASS(obj)   (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_CELL_RENDERER_KEYS, GtkCellRendererKeysClass))

typedef struct _GtkCellRendererKeys      GtkCellRendererKeys;
typedef struct _GtkCellRendererKeysClass GtkCellRendererKeysClass;


typedef enum
{
  GTK_CELL_RENDERER_KEYS_MODE_GTK,
  GTK_CELL_RENDERER_KEYS_MODE_OTHER
} GtkCellRendererKeysMode;


struct _GtkCellRendererKeys
{
  GtkCellRendererText parent;

  /*< private >*/
  guint accel_key;
  GdkModifierType accel_mods;
  guint keycode;
  GtkCellRendererKeysMode accel_mode;

  GtkWidget *edit_widget;
  GtkWidget *grab_widget;
  GtkWidget *sizing_label;
};

struct _GtkCellRendererKeysClass
{
  GtkCellRendererTextClass parent_class;

  void (* accel_edited)  (GtkCellRendererKeys *keys,
		 	  const gchar         *path_string,
			  guint                accel_key,
			  GdkModifierType      accel_mods,
			  guint                hardware_keycode);

  void (* accel_cleared) (GtkCellRendererKeys *keys,
			  const gchar         *path_string);

  /* Padding for future expansion */
  void (*_gtk_reserved0) (void);
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType            gtk_cell_renderer_keys_get_type        (void) G_GNUC_CONST;
GtkCellRenderer *gtk_cell_renderer_keys_new             (void);


G_END_DECLS


#endif /* __GTK_CELL_RENDERER_KEYS_H__ */
