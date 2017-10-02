/* GTK - The GIMP Toolkit
 *
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
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

#ifndef __GSK_CODE_SOURCE_PRIVATE_H__
#define __GSK_CODE_SOURCE_PRIVATE_H__

#include "gsk/gskcodesource.h"

G_BEGIN_DECLS

GskCodeSource *         gsk_code_source_new_for_bytes           (const char             *name,
                                                                 GBytes                 *data);
GskCodeSource *         gsk_code_source_new_for_file            (GFile                  *file);

GBytes *                gsk_code_source_load                    (GskCodeSource          *source,
                                                                 GError                **error);

G_END_DECLS

#endif /* __GSK_CODE_SOURCE_PRIVATE_H__ */
