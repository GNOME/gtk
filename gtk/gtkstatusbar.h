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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_STATUSBAR_H__
#define __GTK_STATUSBAR_H__

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

  GSList *messages;
  GSList *keys;

  guint seq_context_id;
  guint seq_message_id;
};

struct _GtkStatusbarClass
{
  GtkHBoxClass parent_class;

  GMemChunk *messages_mem_chunk;

  void	(*text_pushed)	(GtkStatusbar	*statusbar,
			 guint		 context_id,
			 const gchar	*text);
  void	(*text_popped)	(GtkStatusbar	*statusbar,
			 guint		 context_id,
			 const gchar	*text);
};

struct _GtkStatusbarMsg
{
  gchar *text;
  guint context_id;
  guint message_id;
};

guint      gtk_statusbar_get_type     	(void);
GtkWidget* gtk_statusbar_new          	(void);
guint	   gtk_statusbar_get_context_id	(GtkStatusbar *statusbar,
					 const gchar  *context_description);
/* Returns message_id used for gtk_statusbar_remove */
guint      gtk_statusbar_push          	(GtkStatusbar *statusbar,
					 guint	       context_id,
					 const gchar  *text);
void       gtk_statusbar_pop          	(GtkStatusbar *statusbar,
					 guint	       context_id);
void       gtk_statusbar_remove        	(GtkStatusbar *statusbar,
					 guint	       context_id,
					 guint         message_id);



#ifdef __cplusplus
} 
#endif /* __cplusplus */
#endif /* __GTK_STATUSBAR_H__ */
