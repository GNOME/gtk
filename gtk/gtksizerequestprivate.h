/* gtksizerequestprivate.h
 * Copyright (C) 2010 Openismus GmbH
 *
 * Author:
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
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

#ifndef __GTK_SIZE_REQUEST_PRIVATE_H__
#define __GTK_SIZE_REQUEST_PRIVATE_H__

#include <gtk/gtksizerequest.h>

G_BEGIN_DECLS

typedef struct _GtkRequestedSize GtkRequestedSize;

struct _GtkRequestedSize
{
  gpointer data;
  gint     minimum_size;
  gint     natural_size;
};

gint _gtk_distribute_allocation (gint              extra_space,
				 guint             n_requested_sizes,
				 GtkRequestedSize *sizes);

G_END_DECLS

#endif /* __GTK_SIZE_REQUEST_PRIVATE_H__ */
