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

#ifndef __GTK_IM_CONTEXT_XIM_H__
#define __GTK_IM_CONTEXT_XIM_H__

#include <gtk/gtkimcontext.h>
#include "x11/gdkx.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


extern GType gtk_type_im_context_xim;

#define GTK_TYPE_IM_CONTEXT_XIM              gtk_type_im_context_xim
#define GTK_IM_CONTEXT_XIM(obj)              (GTK_CHECK_CAST ((obj), GTK_TYPE_IM_CONTEXT_XIM, GtkIMContextXIM))
#define GTK_IM_CONTEXT_XIM_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_IM_CONTEXT_XIM, GtkIMContextXIMClass))
#define GTK_IS_IM_CONTEXT_XIM(obj)           (GTK_CHECK_TYPE ((obj), GTK_TYPE_IM_CONTEXT_XIM))
#define GTK_IS_IM_CONTEXT_XIM_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IM_CONTEXT_XIM))
#define GTK_IM_CONTEXT_XIM_GET_CLASS(obj)    (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_IM_CONTEXT_XIM, GtkIMContextXIMClass))


typedef struct _GtkIMContextXIM       GtkIMContextXIM;
typedef struct _GtkIMContextXIMClass  GtkIMContextXIMClass;

typedef struct _GtkXIMInfo GtkXIMInfo;

struct _GtkIMContextXIM
{
  GtkIMContext object;

  GtkXIMInfo *im_info;
  
  gchar *mb_charset;

  GdkWindow *client_window;

  gint preedit_size;
  gint preedit_length;
  gunichar *preedit_chars;
  XIMFeedback *feedbacks;

  gint preedit_cursor;
  
  XIMCallback preedit_start_callback;
  XIMCallback preedit_done_callback;
  XIMCallback preedit_draw_callback;
  XIMCallback preedit_caret_callback;

  XIMCallback status_start_callback;
  XIMCallback status_done_callback;
  XIMCallback status_draw_callback;

  XIC ic;

  guint use_preedit : 1;
};

struct _GtkIMContextXIMClass
{
  GtkIMContextClass parent_class;
};

void gtk_im_context_xim_register_type (GTypeModule *type_module);
GtkIMContext *gtk_im_context_xim_new (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_IM_CONTEXT_XIM_H__ */
