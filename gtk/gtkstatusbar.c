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

#include "gtkframe.h"
#include "gtklabel.h"
#include "gtkstatusbar.h"

static void gtk_statusbar_class_init               (GtkStatusbarClass *class);
static void gtk_statusbar_init                     (GtkStatusbar      *statusbar);
static void gtk_statusbar_destroy                  (GtkObject         *object);
static void gtk_statusbar_show_top_msg             (GtkStatusbar *statusbar);

static GtkContainerClass *parent_class;

guint      
gtk_statusbar_get_type ()
{
  static guint statusbar_type = 0;

  if (!statusbar_type)
    {
      GtkTypeInfo statusbar_info =
      {
        "GtkStatusbar",
        sizeof (GtkStatusbar),
        sizeof (GtkStatusbarClass),
        (GtkClassInitFunc) gtk_statusbar_class_init,
        (GtkObjectInitFunc) gtk_statusbar_init,
        (GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      statusbar_type = gtk_type_unique (gtk_hbox_get_type (), &statusbar_info);
    }

  return statusbar_type;
};

static void
gtk_statusbar_class_init (GtkStatusbarClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  parent_class = gtk_type_class (gtk_box_get_type ());

  object_class->destroy = gtk_statusbar_destroy;

}

static void
gtk_statusbar_init (GtkStatusbar *statusbar)
{
  GTK_BOX (statusbar)->spacing = 2;
  GTK_BOX (statusbar)->homogeneous = FALSE;

  statusbar->frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (statusbar->frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (statusbar), statusbar->frame, 1, 1, 0);
  gtk_widget_show (statusbar->frame);

  statusbar->label = gtk_label_new("");
  gtk_misc_set_alignment(GTK_MISC(statusbar->label), 0.0, 0.0);
  gtk_container_add(GTK_CONTAINER(statusbar->frame), statusbar->label);
  gtk_widget_show (statusbar->label);

  statusbar->next_statusid = 1;
  statusbar->msgs = g_list_alloc();
}

GtkWidget* 
gtk_statusbar_new ()
{
  GtkStatusbar *statusbar;

  statusbar = gtk_type_new (gtk_statusbar_get_type ());

  return GTK_WIDGET (statusbar);
}

gint
gtk_statusbar_push (GtkStatusbar *statusbar, gchar *str)
{
  GtkStatusbarMsg *msg;
  GList *list;

  list = statusbar->msgs;
  msg = g_new(GtkStatusbarMsg, 1);
  msg->str = g_strdup(str);
  msg->statusid = statusbar->next_statusid;
  statusbar->next_statusid++;

  g_list_append(list, msg);

  gtk_statusbar_show_top_msg(statusbar);

  return msg->statusid;
}

static void
gtk_statusbar_show_top_msg (GtkStatusbar *statusbar)
{
  GList *listitem;
  listitem = g_list_last(statusbar->msgs);


  if ((listitem != NULL)  && (listitem->data != NULL)) 
    gtk_label_set(GTK_LABEL(statusbar->label), ((GtkStatusbarMsg*) (listitem->data))->str);

}

void
gtk_statusbar_pop (GtkStatusbar *statusbar, gint statusid) 
{
  GList *listitem;

  listitem = g_list_last(statusbar->msgs);


  while ((listitem != NULL) && (listitem->data != NULL)) {
    
    if (((GtkStatusbarMsg*)(listitem->data))->statusid == statusid) {
      g_list_remove(listitem, listitem->data);
      break;
    }

    listitem = listitem->prev;
  }
 
  gtk_statusbar_show_top_msg(statusbar);
}

static void
gtk_statusbar_destroy (GtkObject *object)
{
  GtkStatusbar *statusbar;
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_STATUSBAR (object));

  statusbar = GTK_STATUSBAR (object);
  g_list_free(statusbar->msgs);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

