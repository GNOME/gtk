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

#ifndef __GTK_IM_MULTICONTEXT_H__
#define __GTK_IM_MULTICONTEXT_H__

#include <gtk/gtkimcontext.h>
#include <gtk/gtkmenushell.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_IM_MULTICONTEXT              (gtk_im_multicontext_get_type ())
#define GTK_IM_MULTICONTEXT(obj)              (GTK_CHECK_CAST ((obj), GTK_TYPE_IM_MULTICONTEXT, GtkIMMulticontext))
#define GTK_IM_MULTICONTEXT_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_IM_MULTICONTEXT, GtkIMMulticontextClass))
#define GTK_IS_IM_MULTICONTEXT(obj)           (GTK_CHECK_TYPE ((obj), GTK_TYPE_IM_MULTICONTEXT))
#define GTK_IS_IM_MULTICONTEXT_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IM_MULTICONTEXT))
#define GTK_IM_MULTICONTEXT_GET_CLASS(obj)    (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_IM_MULTICONTEXT, GtkIMMulticontextClass))


typedef struct _GtkIMMulticontext       GtkIMMulticontext;
typedef struct _GtkIMMulticontextClass  GtkIMMulticontextClass;

struct _GtkIMMulticontext
{
  GtkIMContext object;

  GtkIMContext *slave;

  GdkWindow *client_window;

  const gchar *context_id;
};

struct _GtkIMMulticontextClass
{
  GtkIMContextClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GtkType       gtk_im_multicontext_get_type (void) G_GNUC_CONST;
GtkIMContext *gtk_im_multicontext_new      (void);

void          gtk_im_multicontext_append_menuitems (GtkIMMulticontext *context,
						    GtkMenuShell      *menushell);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_IM_MULTICONTEXT_H__ */
