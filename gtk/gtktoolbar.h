/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkToolbar copyright (C) Federico Mena
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

#ifndef __GTK_TOOLBAR_H__
#define __GTK_TOOLBAR_H__


#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkenums.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TOOLBAR(obj)	 GTK_CHECK_CAST (obj, gtk_toolbar_get_type (), GtkToolbar)
#define GTK_TOOLBAR_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gtk_toolbar_get_type (), GtkToolbarClass)
#define GTK_IS_TOOLBAR(obj)      GTK_CHECK_TYPE (obj, gtk_toolbar_get_type ())


typedef struct _GtkToolbar      GtkToolbar;
typedef struct _GtkToolbarClass GtkToolbarClass;

struct _GtkToolbar
{
  GtkContainer container;

  gint             num_children;
  GList           *children;
  GtkOrientation   orientation;
  GtkToolbarStyle  style;
  gint             space_size; /* big optional space between buttons */

  gint             child_maxw;
  gint             child_maxh;
};

struct _GtkToolbarClass
{
  GtkContainerClass parent_class;
};


guint      gtk_toolbar_get_type      (void);
GtkWidget *gtk_toolbar_new           (GtkOrientation   orientation,
				      GtkToolbarStyle  style);

void       gtk_toolbar_append_item   (GtkToolbar      *toolbar,
				      const char      *text,
				      const char      *tooltip_text,
				      GtkPixmap       *icon);
void       gtk_toolbar_prepend_item  (GtkToolbar      *toolbar,
				      const char      *text,
				      const char      *tooltip_text,
				      GtkPixmap       *icon);
void       gtk_toolbar_insert_item   (GtkToolbar      *toolbar,
				      const char      *text,
				      const char      *tooltip_text,
				      GtkPixmap       *icon,
				      gint             position);
void       gtk_toolbar_append_space  (GtkToolbar      *toolbar);
void       gtk_toolbar_prepend_space (GtkToolbar      *toolbar);
void       gtk_toolbar_insert_space  (GtkToolbar      *toolbar,
				      gint             position);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_TOOLBAR_H__ */
