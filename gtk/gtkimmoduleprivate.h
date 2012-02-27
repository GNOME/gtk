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

#ifndef __GTK_IM_MODULE_PRIVATE_H__
#define __GTK_IM_MODULE_PRIVATE_H__

#include <gdk/gdk.h>
#include <gtk/gtkimcontext.h>
#include <gtk/gtkimcontextinfo.h>

G_BEGIN_DECLS

void           _gtk_im_module_list                   (const GtkIMContextInfo ***contexts,
						      guint                    *n_contexts);
GtkIMContext * _gtk_im_module_create                 (const gchar              *context_id);
const gchar  * _gtk_im_module_get_default_context_id (GdkWindow                *client_window);

G_END_DECLS

#endif /* __GTK_IM_MODULE_PRIVATE_H__ */
