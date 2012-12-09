/*
 * Gtk.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corporation.
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
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

#ifndef __GTK_BIN_LAYOUT_PRIVATE_H__
#define __GTK_BIN_LAYOUT_PRIVATE_H__

#include <gtk/actors/gtklayoutmanagerprivate.h>

G_BEGIN_DECLS

#define GTK_TYPE_BIN_LAYOUT                 (_gtk_bin_layout_get_type ())
#define GTK_BIN_LAYOUT(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_BIN_LAYOUT, GtkBinLayout))
#define GTK_IS_BIN_LAYOUT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_BIN_LAYOUT))
#define GTK_BIN_LAYOUT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_BIN_LAYOUT, GtkBinLayoutClass))
#define GTK_IS_BIN_LAYOUT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_BIN_LAYOUT))
#define GTK_BIN_LAYOUT_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_BIN_LAYOUT, GtkBinLayoutClass))

typedef struct _GtkBinLayout                GtkBinLayout;
typedef struct _GtkBinLayoutPrivate         GtkBinLayoutPrivate;
typedef struct _GtkBinLayoutClass           GtkBinLayoutClass;

/**
 * GtkBinLayout:
 *
 * The #GtkBinLayout structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _GtkBinLayout
{
  /*< private >*/
  GtkLayoutManager parent_instance;

  GtkBinLayoutPrivate *priv;
};

/**
 * GtkBinLayoutClass:
 *
 * The #GtkBinLayoutClass structure contains only private
 * data and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _GtkBinLayoutClass
{
  /*< private >*/
  GtkLayoutManagerClass parent_class;
};

GType                   _gtk_bin_layout_get_type                 (void) G_GNUC_CONST;

GtkLayoutManager *      _gtk_bin_layout_new                      (void);


G_END_DECLS

#endif /* __GTK_BIN_LAYOUT_H__ */
