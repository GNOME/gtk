/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkStatusbar Copyright (C) 1998 Shawn T. Amundson
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

#ifndef __GTK_STATUSBAR_H__
#define __GTK_STATUSBAR_H_

#include <gtk/gtkhbox.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_STATUSBAR(obj)          GTK_CHECK_CAST (obj, gtk_statusbar_get_type (), GtkStatusbar)
#define GTK_STATUSBAR_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_statusbar_get_type (), GtkStatusbarClass)
#define GTK_IS_STATUSBAR(obj)       GTK_CHECK_TYPE (obj, gtk_statusbar_get_type ())

typedef struct _GtkStatusbar      GtkStatusbar;
typedef struct _GtkStatusbarClass GtkStatusbarClass;
typedef struct _GtkStatusbarMsg GtkStatusbarMsg;

struct _GtkStatusbar
{
  GtkHBox parent_widget;

  GtkWidget *frame;
  GtkWidget *label;

  GList *messages;

  guint seq_status_id;
};

struct _GtkStatusbarClass
{
  GtkHBoxClass parent_class;

  GMemChunk *messages_mem_chunk;

  void	(*text_pushed)	(GtkStatusbar	*statusbar,
			 const gchar	*text);
  void	(*text_popped)	(GtkStatusbar	*statusbar,
			 const gchar	*text);
};

struct _GtkStatusbarMsg
{
  gchar *text;
  guint status_id;
};

guint      gtk_statusbar_get_type     	(void);
GtkWidget* gtk_statusbar_new          	(void);

/* Returns StatusID used for gtk_statusbar_push */
guint      gtk_statusbar_push          	(GtkStatusbar *statusbar, 
					 const gchar  *text);
void       gtk_statusbar_pop          	(GtkStatusbar *statusbar);
void       gtk_statusbar_steal         	(GtkStatusbar *statusbar, 
					 guint          status_id);



#ifdef __cplusplus
} 
#endif /* __cplusplus */
#endif /* __GTK_STATUSBAR_H_ */
