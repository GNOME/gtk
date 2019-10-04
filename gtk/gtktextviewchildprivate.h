/* GTK - The GIMP Toolkit
 * gtktextviewchild-private.h Copyright (C) 2019 Red Hat, Inc.
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

#ifndef __GTK_TEXT_VIEW_CHILD_PRIVATE_H__
#define __GTK_TEXT_VIEW_CHILD_PRIVATE_H__

#include "gtktextviewchild.h"

void _gtk_text_view_child_set_offset (GtkTextViewChild *child,
                                      int               xoffset,
                                      int               yoffset);

#endif /* __GTK_TEXT_VIEW_CHILD_PRIVATE_H__ */
