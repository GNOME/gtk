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
#ifndef __GTK_EDITABLE_H__
#define __GTK_EDITABLE_H__


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_EDITABLE(obj)          GTK_CHECK_CAST (obj, gtk_editable_get_type (), GtkEditable)
#define GTK_EDITABLE_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_editable_get_type (), GtkEditableClass)
#define GTK_IS_EDITABLE(obj)       GTK_CHECK_TYPE (obj, gtk_editable_get_type ())


typedef struct _GtkEditable       GtkEditable;
typedef struct _GtkEditableClass  GtkEditableClass;

typedef void (*GtkTextFunction) (GtkEditable  *editable, guint32 time);

struct _GtkEditable
{
  GtkWidget widget;

  guint   current_pos;

  guint   selection_start_pos;
  guint   selection_end_pos;
  guint   has_selection : 1;
  guint   editable : 1;
  GdkIC   ic;

  gchar *clipboard_text;
};

struct _GtkEditableClass
{
  GtkWidgetClass parent_class;

  void (* insert_text)  (GtkEditable    *editable,
			 const gchar    *text,
			 gint            length,
			 gint           *position);
  void (* delete_text)  (GtkEditable    *editable,
			 gint            start_pos,
			 gint            end_pos);
  void (* update_text)  (GtkEditable    *editable,
			 gint            start_pos,
			 gint            end_pos);
  gchar* (* get_chars)  (GtkEditable    *editable,
			 gint            start_pos,
			 gint            end_pos);
  void (* set_selection)(GtkEditable    *editable,
			 gint            start_pos,
			 gint            end_pos);
  void (* activate)     (GtkEditable    *editable);
  void (* changed)      (GtkEditable    *editable);
};

guint      gtk_editable_get_type       (void);
void       gtk_editable_select_region  (GtkEditable      *editable,
					gint              start,
					gint              end);
void       gtk_editable_insert_text   (GtkEditable       *editable,
					const gchar      *new_text,
					gint              new_text_length,
					gint             *position);
void       gtk_editable_delete_text    (GtkEditable      *editable,
					gint              start_pos,
					gint              end_pos);
gchar *    gtk_editable_get_chars      (GtkEditable      *editable,
					gint              start_pos,
					gint              end_pos);
void       gtk_editable_cut_clipboard  (GtkEditable      *editable,
					guint32           time);
void       gtk_editable_copy_clipboard (GtkEditable      *editable, 
					guint32           time);
void       gtk_editable_paste_clipboard (GtkEditable     *editable, 
					 guint32          time);
void       gtk_editable_claim_selection (GtkEditable     *editable, 
					 gboolean         claim, 
					 guint32          time);
void       gtk_editable_delete_selection (GtkEditable    *editable);

void       gtk_editable_changed         (GtkEditable    *editable);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_EDITABLE_H__ */
