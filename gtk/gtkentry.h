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
#include <gtk/gtkeditable.h>


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
  GtkEditable editable;

  GdkWindow *text_area;
  GdkPixmap *backing_pixmap;
  GdkCursor *cursor;
  gchar *text;

  guint16 text_size;
  guint16 text_length;
  guint16 text_max_length;
  gint16  scroll_offset;
  guint   visible : 1;
  guint32 timer;
};

struct _GtkEntryClass
{
  GtkEditableClass parent_class;

  void (* activate)     (GtkEntry    *entry);
};

guint      gtk_entry_get_type       (void);
GtkWidget* gtk_entry_new            (void);
GtkWidget* gtk_entry_new_with_max_length (guint16   max);
void       gtk_entry_set_text       (GtkEntry      *entry,
				     const gchar   *text);
void       gtk_entry_append_text    (GtkEntry      *entry,
				     const gchar   *text);
void       gtk_entry_prepend_text   (GtkEntry      *entry,
				     const gchar   *text);
void       gtk_entry_set_position   (GtkEntry      *entry,
				     gint           position);
gchar*     gtk_entry_get_text       (GtkEntry      *entry);
void       gtk_entry_select_region  (GtkEntry      *entry,
				     guint          start,
				     guint          end);
void       gtk_entry_set_visibility (GtkEntry      *entry,
				     gboolean       visible);
void       gtk_entry_set_editable   (GtkEntry      *entry,
				     gboolean       editable);

/* If entry->text is already > max it's up to you to change it */
void       gtk_entry_set_max_length (GtkEntry      *entry,
                                     guint16        max);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ENTRY_H__ */
