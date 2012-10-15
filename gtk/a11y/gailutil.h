/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GAIL_UTIL_H__
#define __GAIL_UTIL_H__

#include <atk/atk.h>

G_BEGIN_DECLS

void _gail_util_install   (void);
void _gail_util_uninstall (void);

gboolean  _gail_util_key_snooper (GtkWidget   *the_widget,
                                  GdkEventKey *event);

G_END_DECLS

#endif /* __GAIL_UTIL_H__ */
