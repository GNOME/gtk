/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat Software
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_IM_CONTEXT_H__
#define __GTK_IM_CONTEXT_H__

#include <gdk/gdk.h>
#include <gtk/gtkobject.h>
#include <pango/pango.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_IM_CONTEXT              (gtk_im_context_get_type ())
#define GTK_IM_CONTEXT(obj)              (GTK_CHECK_CAST ((obj), GTK_TYPE_IM_CONTEXT, GtkIMContext))
#define GTK_IM_CONTEXT_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_IM_CONTEXT, GtkIMContextClass))
#define GTK_IS_IM_CONTEXT(obj)           (GTK_CHECK_TYPE ((obj), GTK_TYPE_IM_CONTEXT))
#define GTK_IS_IM_CONTEXT_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IM_CONTEXT))
#define GTK_IM_CONTEXT_GET_CLASS(obj)    (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_IM_CONTEXT, GtkIMContextClass))


typedef struct _GtkIMContext       GtkIMContext;
typedef struct _GtkIMContextClass  GtkIMContextClass;

struct _GtkIMContext
{
  GtkObject object;
};

struct _GtkIMContextClass
{
  GtkObjectClass parent_class;

  /* Signals */
  void (*preedit_start)   (GtkIMContext *context);
  void (*preedit_end)     (GtkIMContext *context);
  void (*preedit_changed) (GtkIMContext *context);
  void (*commit)          (GtkIMContext *context, const gchar *str);

  /* Virtual functions */
  void     (*set_client_window)  (GtkIMContext   *context,
				  GdkWindow      *window);
  void     (*get_preedit_string) (GtkIMContext   *context,
				  gchar         **str,
				  PangoAttrList **attrs,
				  gint           *cursor_pos);
  gboolean (*filter_keypress)    (GtkIMContext   *context,
			          GdkEventKey    *event);
  void     (*focus_in)           (GtkIMContext   *context);
  void     (*focus_out)          (GtkIMContext   *context);
  void     (*reset)              (GtkIMContext   *context);
  void     (*set_cursor_location) (GtkIMContext   *context,
				   GdkRectangle	 *area);
};

GtkType       gtk_im_context_get_type           (void) G_GNUC_CONST;

void          gtk_im_context_set_client_window  (GtkIMContext   *context,
						 GdkWindow      *window);
void          gtk_im_context_get_preedit_string (GtkIMContext   *context,
						 gchar         **str,
						 PangoAttrList **attrs,
						 gint           *cursor_pos);
gboolean      gtk_im_context_filter_keypress    (GtkIMContext   *context,
						 GdkEventKey    *event);
void          gtk_im_context_focus_in           (GtkIMContext   *context);
void          gtk_im_context_focus_out          (GtkIMContext   *context);
void          gtk_im_context_reset              (GtkIMContext   *context);
void          gtk_im_context_set_cursor_location (GtkIMContext   *context,
						  GdkRectangle	*area);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_IM_CONTEXT_H__ */
