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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_ENTRY_H__
#define __GTK_ENTRY_H__


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_ENTRY(obj)          GTK_CHECK_CAST (obj, gtk_entry_get_type (), GtkEntry)
#define GTK_ENTRY_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_entry_get_type (), GtkEntryClass)
#define GTK_IS_ENTRY(obj)       GTK_CHECK_TYPE (obj, gtk_entry_get_type ())


typedef struct _GtkEntry       GtkEntry;
typedef struct _GtkEntryClass  GtkEntryClass;

struct _GtkEntry
{
  GtkWidget widget;

  GdkWindow *text_area;
  gchar *text;

  guint16  text_size;
  guint16  text_length;
  gint16   current_pos;
  gint16   selection_start_pos;
  gint16   selection_end_pos;
  gint16   scroll_offset;
  gboolean have_selection : 1;
  gboolean visible : 1;
  guint32  timer;
  GdkIC    ic;

  gchar *clipboard_text;
};

struct _GtkEntryClass
{
  GtkWidgetClass parent_class;

  void (* insert_text)  (GtkEntry    *entry,
			 const gchar *text,
			 gint         length,
			 gint        *position);
  void (* delete_text)  (GtkEntry    *entry,
			 gint         start_pos,
			 gint         end_pos);
  void (* changed)      (GtkEntry    *entry);
  void (* set_text)     (GtkEntry    *entry);
  void (* activate)     (GtkEntry    *entry);
};


guint      gtk_entry_get_type     (void);
GtkWidget* gtk_entry_new          (void);
void       gtk_entry_set_text     (GtkEntry      *entry,
				   const gchar   *text);
void       gtk_entry_append_text  (GtkEntry      *entry,
				   const gchar   *text);
void       gtk_entry_prepend_text (GtkEntry      *entry,
				   const gchar   *text);
void       gtk_entry_set_position (GtkEntry      *entry,
				   gint           position);
gchar*     gtk_entry_get_text     (GtkEntry      *entry);
void       gtk_entry_set_visibility (GtkEntry    *entry,
				     gint           visible);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ENTRY_H__ */
