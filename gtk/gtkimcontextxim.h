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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_IM_CONTEXT_XIM_H__
#define __GTK_IM_CONTEXT_XIM_H__

#include <gtk/gtk.h>
#include "x11/gdkx.h"

G_BEGIN_DECLS

#define GTK_TYPE_IM_CONTEXT_XIM            (gtk_im_context_xim_get_type ())
#define GTK_IM_CONTEXT_XIM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_IM_CONTEXT_XIM, GtkIMContextXIM))
#define GTK_IM_CONTEXT_XIM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_IM_CONTEXT_XIM, GtkIMContextXIMClass))
#define GTK_IS_IM_CONTEXT_XIM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_IM_CONTEXT_XIM))
#define GTK_IS_IM_CONTEXT_XIM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IM_CONTEXT_XIM))
#define GTK_IM_CONTEXT_XIM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_IM_CONTEXT_XIM, GtkIMContextXIMClass))


typedef struct _GtkIMContextXIM       GtkIMContextXIM;
typedef struct _GtkIMContextXIMClass  GtkIMContextXIMClass;

struct _GtkIMContextXIMClass
{
  GtkIMContextClass parent_class;
};

GType         gtk_im_context_xim_get_type (void) G_GNUC_CONST;
void          gtk_im_context_xim_shutdown (void);

G_END_DECLS

#endif /* __GTK_IM_CONTEXT_XIM_H__ */
